#include "Model.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

using namespace Microsoft::WRL;

std::unique_ptr<Model> Model::CreateFromOBJ(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    std::unique_ptr<Model> model = std::make_unique<Model>();
    model->Initialize(dxCommon, directoryPath, filename, textureManager);
    return model;
}

void Model::Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename, TextureManager* textureManager) {
    // OBJから頂点、法線、UV、インデックス、MTLのテクスチャパスを読み込む。
    LoadObjFile(directoryPath, filename);

    // 頂点がないモデルはGPUバッファを作れないため、描画不能モデルとして早期終了する。
    if (vertices_.empty()) {
        OutputDebugStringA("Error: Model vertices are empty!\n");
        return;
    }

    // CPU側で読み込んだ頂点/インデックスをGPU用バッファへ転送する。
    CreateBuffers(dxCommon);

    // MTLから見つかったテクスチャをTextureManagerへ登録する。
    if (textureManager) {
        textureHandle_ = textureManager->LoadTexture(textureFilePath_);
    }
}

void Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    const std::string fullPath = directoryPath + "/" + filename;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        const std::string message = "Failed to open OBJ file: " + fullPath;
        OutputDebugStringA((message + "\n").c_str());
        throw std::runtime_error(message);
    }

    OutputDebugStringA(("---- Loading: " + fullPath + " ----\n").c_str());

    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::unordered_map<std::string, uint32_t> vertexIndexMap;
    std::string line;
    int lineCount = 0;

    vertices_.clear();
    indices_.clear();

    while (std::getline(file, line)) {
        lineCount++;
        std::stringstream lineStream(line);
        std::string identifier;
        lineStream >> identifier;

        if (identifier == "v") {
            // OBJの位置座標。w=1.0を補って同次座標として保持する。
            Vector4 position;
            lineStream >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        } else if (identifier == "vt") {
            // OBJとDirectXでV方向が逆なので、読み込み時にYを反転する。
            Vector2 texcoord;
            lineStream >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        } else if (identifier == "vn") {
            Vector3 normal;
            lineStream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (identifier == "f") {
            // faceは三角形前提。position/texcoord/normal の組み合わせを1頂点として重複をまとめる。
            for (int faceVertexIndex = 0; faceVertexIndex < 3; faceVertexIndex++) {
                std::string faceVertexToken;
                lineStream >> faceVertexToken;

                auto existing = vertexIndexMap.find(faceVertexToken);
                if (existing != vertexIndexMap.end()) {
                    indices_.push_back(existing->second);
                    continue;
                }

                std::stringstream faceVertexStream(faceVertexToken);
                std::string componentIndexText;
                int positionIndex = 0;
                int texcoordIndex = 0;
                int normalIndex = 0;

                std::getline(faceVertexStream, componentIndexText, '/');
                if (!componentIndexText.empty()) {
                    positionIndex = std::stoi(componentIndexText) - 1;
                }
                std::getline(faceVertexStream, componentIndexText, '/');
                if (!componentIndexText.empty()) {
                    texcoordIndex = std::stoi(componentIndexText) - 1;
                }
                std::getline(faceVertexStream, componentIndexText, '/');
                if (!componentIndexText.empty()) {
                    normalIndex = std::stoi(componentIndexText) - 1;
                }

                ModelVertexData vertex;
                vertex.position = positions[positionIndex];
                if (texcoordIndex >= 0 && texcoordIndex < static_cast<int>(texcoords.size())) {
                    vertex.texcoord = texcoords[texcoordIndex];
                }
                if (normalIndex >= 0 && normalIndex < static_cast<int>(normals.size())) {
                    vertex.normal = normals[normalIndex];
                }

                // OBJは右手座標系、エンジンは左手座標系なのでZ軸を反転する。
                vertex.position.z *= -1.0f;
                vertex.normal.z *= -1.0f;

                const uint32_t newIndex = static_cast<uint32_t>(vertices_.size());
                vertexIndexMap.emplace(faceVertexToken, newIndex);
                vertices_.push_back(vertex);
                indices_.push_back(newIndex);
            }
        } else if (identifier == "mtllib") {
            std::string materialFilename;
            lineStream >> materialFilename;
            LoadMaterialFile(directoryPath, materialFilename);
        }
    }

    const std::string report =
        "Read Lines: " + std::to_string(lineCount) +
        ", Parsed Vertices: " + std::to_string(vertices_.size()) +
        ", Parsed Indices: " + std::to_string(indices_.size()) + "\n";
    OutputDebugStringA(report.c_str());

    if (vertices_.empty()) {
        OutputDebugStringA(("Error: Model vertices are empty! File: " + filename + "\n").c_str());
    }
}

void Model::CreateBuffers(DirectXCommon* dxCommon) {
    auto device = dxCommon->GetDevice();
    const UINT vertexBufferSize = static_cast<UINT>(sizeof(ModelVertexData) * vertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.SampleDesc.Count = 1;

    // 頂点バッファをUpload Heapに作成し、CPUから直接書き込めるようにする。
    HRESULT resultCode = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    if (FAILED(resultCode)) {
        assert(false && "Failed to create Vertex Buffer");
        return;
    }

    ModelVertexData* mappedVertexBuffer = nullptr;
    resultCode = vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexBuffer));
    if (SUCCEEDED(resultCode)) {
        std::copy(vertices_.begin(), vertices_.end(), mappedVertexBuffer);
        vertexBuffer_->Unmap(0, nullptr);
    }

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = vertexBufferSize;
    vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);

    if (!indices_.empty()) {
        const UINT indexBufferSize = static_cast<UINT>(sizeof(uint32_t) * indices_.size());
        resourceDesc.Width = indexBufferSize;

        resultCode = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer_));

        if (FAILED(resultCode)) {
            assert(false && "Failed to create Index Buffer");
            return;
        }

        uint32_t* mappedIndexBuffer = nullptr;
        resultCode = indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndexBuffer));
        if (SUCCEEDED(resultCode)) {
            std::copy(indices_.begin(), indices_.end(), mappedIndexBuffer);
            indexBuffer_->Unmap(0, nullptr);
        }

        indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = indexBufferSize;
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }
}

void Model::LoadMaterialFile(const std::string& directoryPath, const std::string& filename) {
    const std::string fullPath = directoryPath + "/" + filename;
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        OutputDebugStringA(("Warning: Failed to open material file: " + fullPath + "\n").c_str());
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream lineStream(line);
        std::string identifier;
        lineStream >> identifier;

        if (identifier == "map_Kd") {
            // map_Kd は拡散反射テクスチャ。現在のModelはこの1枚を描画テクスチャとして使う。
            std::string textureFilename;
            lineStream >> textureFilename;

            textureFilePath_ = directoryPath + "/" + textureFilename;
            OutputDebugStringA(("Texture from MTL: " + textureFilePath_ + "\n").c_str());
            return;
        }
    }
}

void Model::InitializeFromVertices(DirectXCommon* dxCommon, const std::vector<ModelVertexData>& vertices, uint32_t textureHandle) {
    // OBJを介さず、生成済み頂点データからModelを作る。スキニングや手続き型メッシュで使う。
    vertices_ = vertices;
    indices_.clear();
    textureHandle_ = textureHandle;

    if (vertices_.empty()) {
        OutputDebugStringA("Error: Vertices empty in InitializeFromVertices!\n");
        return;
    }

    CreateBuffers(dxCommon);
}

void Model::UpdateVertexBuffer(const std::vector<ModelVertexData>& vertices) {
    // 頂点数は変えず、中身だけ更新する用途。スキニング済み頂点の反映などで使う。
    if (vertices.size() != vertices_.size()) {
        OutputDebugStringA("Warning: Vertex count mismatch in UpdateVertexBuffer!\n");
        vertices_ = vertices;
        return;
    }

    vertices_ = vertices;
    if (!vertexBuffer_) {
        return;
    }

    ModelVertexData* mappedVertexBuffer = nullptr;
    HRESULT resultCode = vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexBuffer));
    if (SUCCEEDED(resultCode)) {
        std::copy(vertices_.begin(), vertices_.end(), mappedVertexBuffer);
        vertexBuffer_->Unmap(0, nullptr);
    }
}

void Model::Draw(ID3D12GraphicsCommandList* commandList) {
    // バッファがないモデルは描画できないため、何もせず戻る。
    if (!vertexBuffer_) {
        return;
    }

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    if (indexBuffer_ && !indices_.empty()) {
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(UINT(indices_.size()), 1, 0, 0, 0);
    } else {
        commandList->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
    }
}

void Model::DrawInstanced(ID3D12GraphicsCommandList* commandList, UINT instanceCount) {
    if (!vertexBuffer_ || instanceCount == 0) {
        return;
    }

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    if (indexBuffer_ && !indices_.empty()) {
        commandList->IASetIndexBuffer(&indexBufferView_);
        commandList->DrawIndexedInstanced(UINT(indices_.size()), instanceCount, 0, 0, 0);
    } else {
        commandList->DrawInstanced(UINT(vertices_.size()), instanceCount, 0, 0);
    }
}
