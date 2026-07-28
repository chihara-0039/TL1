#include "Skybox.h"
#include "TextureManager.h"
#include <cassert>
#include <vector>

void Skybox::Initialize(Object3dCommon* object3dCommon, uint32_t textureHandle) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
    textureHandle_ = textureHandle;

    auto device = object3dCommon_->GetDxCommon()->GetDevice();

    // 1. メッシュ作成
    CreateMesh();

    // 2. 定数バッファ作成 (Transform)
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    resDesc.Height = 1; 
    resDesc.DepthOrArraySize = 1; 
    resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; 
    resDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&transformationResource_));
    assert(SUCCEEDED(hr));
    transformationResource_->Map(0, nullptr, (void**)&transformationData_);
    transformationData_->WVP = Math::MakeIdentity4x4();
    transformationData_->World = Math::MakeIdentity4x4();

    // 3. 定数バッファ作成 (Material)
    resDesc.Width = (sizeof(Material) + 0xff) & ~0xff;
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialResource_));
    assert(SUCCEEDED(hr));
    materialResource_->Map(0, nullptr, (void**)&materialData_);
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0; // Skyboxはライティング不要
    materialData_->shininess = 0.0f;
    materialData_->metallic = 0.0f;
    materialData_->emissive = 0.0f;
    materialData_->uvTransform = Math::MakeIdentity4x4();

    // 4. PSO作成
    CreateGraphicsPipeline();

    // デフォルトスケールを設定 (十分に大きく、farClip内に収まるように)
    transform_.scale = { 50.0f, 50.0f, 50.0f };
}

void Skybox::CreateMesh() {
    auto device = object3dCommon_->GetDxCommon()->GetDevice();

    struct Vertex {
        Vector4 position;
    };

    // 24頂点 (6面 * 4頂点)
    Vertex vertices[24];

    // 右面 (+X)
    vertices[0].position = { 1.0f,  1.0f,  1.0f, 1.0f };
    vertices[1].position = { 1.0f,  1.0f, -1.0f, 1.0f };
    vertices[2].position = { 1.0f, -1.0f,  1.0f, 1.0f };
    vertices[3].position = { 1.0f, -1.0f, -1.0f, 1.0f };

    // 左面 (-X)
    vertices[4].position = { -1.0f,  1.0f, -1.0f, 1.0f };
    vertices[5].position = { -1.0f,  1.0f,  1.0f, 1.0f };
    vertices[6].position = { -1.0f, -1.0f, -1.0f, 1.0f };
    vertices[7].position = { -1.0f, -1.0f,  1.0f, 1.0f };

    // 前面 (+Z)
    vertices[8].position  = { -1.0f,  1.0f,  1.0f, 1.0f };
    vertices[9].position  = {  1.0f,  1.0f,  1.0f, 1.0f };
    vertices[10].position = { -1.0f, -1.0f,  1.0f, 1.0f };
    vertices[11].position = {  1.0f, -1.0f,  1.0f, 1.0f };

    // 後面 (-Z)
    vertices[12].position = {  1.0f,  1.0f, -1.0f, 1.0f };
    vertices[13].position = { -1.0f,  1.0f, -1.0f, 1.0f };
    vertices[14].position = {  1.0f, -1.0f, -1.0f, 1.0f };
    vertices[15].position = { -1.0f, -1.0f, -1.0f, 1.0f };

    // 上面 (+Y)
    vertices[16].position = {  1.0f,  1.0f,  1.0f, 1.0f };
    vertices[17].position = { -1.0f,  1.0f,  1.0f, 1.0f };
    vertices[18].position = {  1.0f,  1.0f, -1.0f, 1.0f };
    vertices[19].position = { -1.0f,  1.0f, -1.0f, 1.0f };

    // 下面 (-Y)
    vertices[20].position = { -1.0f, -1.0f,  1.0f, 1.0f };
    vertices[21].position = {  1.0f, -1.0f,  1.0f, 1.0f };
    vertices[22].position = { -1.0f, -1.0f, -1.0f, 1.0f };
    vertices[23].position = {  1.0f, -1.0f, -1.0f, 1.0f };

    // 頂点バッファ作成
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeof(vertices);
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource_));
    assert(SUCCEEDED(hr));

    void* pVertexData = nullptr;
    vertexResource_->Map(0, nullptr, &pVertexData);
    memcpy(pVertexData, vertices, sizeof(vertices));
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(vertices);
    vertexBufferView_.StrideInBytes = sizeof(Vertex);

    // インデックスバッファ作成 (36インデックス)
    uint16_t indices[36];
    for (uint16_t i = 0; i < 6; ++i) {
        uint16_t offset = i * 4;
        indices[i * 6 + 0] = offset + 0;
        indices[i * 6 + 1] = offset + 1;
        indices[i * 6 + 2] = offset + 2;
        indices[i * 6 + 3] = offset + 2;
        indices[i * 6 + 4] = offset + 1;
        indices[i * 6 + 5] = offset + 3;
    }

    resDesc.Width = sizeof(indices);
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexResource_));
    assert(SUCCEEDED(hr));

    void* pIndexData = nullptr;
    indexResource_->Map(0, nullptr, &pIndexData);
    memcpy(pIndexData, indices, sizeof(indices));
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(indices);
    indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
}

void Skybox::CreateGraphicsPipeline() {
    auto device = object3dCommon_->GetDxCommon()->GetDevice();

    auto vsBlob = object3dCommon_->GetDxCommon()->CompileShader(L"Resources/shaders/hlsl/Skybox.VS.hlsl", L"vs_6_0");
    auto psBlob = object3dCommon_->GetDxCommon()->CompileShader(L"Resources/shaders/hlsl/Skybox.PS.hlsl", L"ps_6_0");
    assert(vsBlob && psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = object3dCommon_->GetRootSignature();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void Skybox::Update() {
    Matrix4x4 worldMatrix = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix_, projectionMatrix_));

    transformationData_->WVP = wvpMatrix;
    transformationData_->World = worldMatrix;
}

void Skybox::Update(const Vector3& cameraPosition) {
    transform_.translate = cameraPosition;
    Update();
}

void Skybox::Draw() {
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
    TextureManager* textureManager = object3dCommon_->GetTextureManager();

    if (!textureManager) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(object3dCommon_->GetRootSignature());

    // 0: Material
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    // 1: Transform
    commandList->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());

    // 3: Texture (Cubemap)
    auto gpuHandle = textureManager->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}
