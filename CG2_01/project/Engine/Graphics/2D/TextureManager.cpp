#include "TextureManager.h"
#include <DirectXTex.h>
#include <vector>
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

// 文字列変換用
static std::wstring ConvertString(const std::string& str) {

	// 文字列が空の場合は空のワイド文字列を返す
    if (str.empty()) { 

		// ここで失敗を検知して止める
        return std::wstring();
    }

	// UTF-8からUTF-16への変換
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	// 変換後のワイド文字列を格納するためのバッファを確保
    std::wstring wstrTo(size_needed, 0);
	// 変換を実行
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	// 変換後のワイド文字列を返す
    return wstrTo;
}


// データ転送関数
[[nodiscard]]

// テクスチャデータをアップロードするための関数。CPUアクセス可能なリソースを作成し、ミップマップごとにデータを転送していく。
ComPtr<ID3D12Resource> UploadTextureData(ID3D12Device* device, const DirectX::ScratchImage& mipImages) {
	
    // テクスチャのメタデータからリソースの説明を作成
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// D3D12_RESOURCE_DESC構造体を初期化して、テクスチャの幅、高さ、ミップレベル数、フォーマットなどを設定
    D3D12_RESOURCE_DESC textureDesc{};
	// テクスチャの幅をDirectXTexのメタデータから取得して設定
    textureDesc.Width = UINT(metadata.width);
	// テクスチャの高さをDirectXTexのメタデータから取得して設定
    textureDesc.Height = UINT(metadata.height);
	// ミップレベル数はDirectXTexのメタデータから取得
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
	// 3Dテクスチャの場合はDepthを設定し、2Dテクスチャの場合はArraySizeを設定する。DirectXTexのメタデータから取得
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	// フォーマットはDirectXTexのメタデータから取得
    textureDesc.Format = metadata.format;
	// サンプル数は通常1で、マルチサンプリングを使用しない場合は0に設定
    textureDesc.SampleDesc.Count = 1;
	// テクスチャの次元を設定（1D、2D、3Dなど）
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	// アップロード用のリソースを作成するためのヒーププロパティを設定。アップロードヒープはCPUから書き込み可能で、GPUからは読み取り専用のリソースを作成するために使用される。
    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_CUSTOM, D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 1, 1 };

	// CPUアクセス可能なリソースを作成。D3D12_RESOURCE_STATE_GENERIC_READは、GPUがこのリソースを読み取るための状態であることを示す。
    ComPtr<ID3D12Resource> resource;
	// CreateCommittedResource関数を使用して、アップロード用のリソースを作成する。ヒーププロパティ、リソースの説明、初期状態などを指定する。
    HRESULT hr = device->CreateCommittedResource(
		// ヒーププロパティを指定。アップロードヒープはCPUから書き込み可能で、GPUからは読み取り専用のリソースを作成するために使用される。
        &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
		// 初期状態はD3D12_RESOURCE_STATE_GENERIC_READで、GPUがこのリソースを読み取るための状態であることを示す。
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	// リソースの作成に失敗した場合は、HRESULTをチェックしてエラーを検出する。失敗した場合は、assertで強制停止させる。
    assert(SUCCEEDED(hr));

	// ミップマップごとにデータを転送するためのループ。DirectXTexのScratchImageからミップマップのイメージデータを取得し、リソースにマッピングしてデータをコピーする。
    const DirectX::Image* intermediateImages = mipImages.GetImages();

	// 各ミップレベルに対して、リソースをマッピングしてデータをコピーする。Map関数を使用してリソースのサブリソースをマッピングし、ポインタを取得する。
    for (size_t i = 0; i < metadata.mipLevels; ++i) {
		// ミップマップのイメージデータを取得
        const DirectX::Image& img = intermediateImages[i];
		// リソースのサブリソースをマッピングして、CPUがアクセスできるポインタを取得する。Map関数は、リソースの特定のサブリソースをマッピングし、データへのポインタを返す。
        void* pData = nullptr;
		// Map関数を呼び出して、リソースのサブリソースをマッピングする。UINT(i)は、マップするサブリソースのインデックスを指定する。nullptrは、読み取り範囲を指定するためのD3D12_RANGE構造体へのポインタで、ここでは全体をマッピングするためにnullptrを指定している。&pDataは、マッピングされたデータへのポインタを受け取るための引数。
        hr = resource->Map(UINT(i), nullptr, &pData);

		// マッピングに成功した場合は、イメージデータをリソースにコピーする。img.pixelsは、DirectXTexのImage構造体からピクセルデータへのポインタを取得する。dstは、マッピングされたリソースのデータへのポインタで、pDataから取得する。ループを使用して、各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
        if (SUCCEEDED(hr)) {
			// データをコピーするためのループ。各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
            const uint8_t* src = img.pixels;
			// コピー先のポインタをuint8_t*にキャストして、行ごとにデータをコピーする。memcpy関数を使用して、srcからdstにimg.rowPitchバイト分のデータをコピーする。ループは、テクスチャの高さ分だけ繰り返される。
            uint8_t* dst = static_cast<uint8_t*>(pData);
			// 各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
            for (size_t y = 0; y < img.height; ++y) {
				// memcpy関数を使用して、srcからdstにimg.rowPitchバイト分のデータをコピーする。ループは、テクスチャの高さ分だけ繰り返される。
                memcpy(dst, src, img.rowPitch);
				// コピー後、srcとdstのポインタを次の行に進めるために、img.rowPitchバイト分だけ増加させる。これにより、次の行のデータがコピーされる。
                src += img.rowPitch;
                dst += img.rowPitch;
            }
			// データのコピーが完了したら、リソースのサブリソースをアンマップする。Unmap関数を呼び出して、リソースのサブリソースをアンマップする。UINT(i)は、アンマップするサブリソースのインデックスを指定する。nullptrは、書き込み範囲を指定するためのD3D12_RANGE構造体へのポインタで、ここでは全体をアンマップするためにnullptrを指定している。
            resource->Unmap(UINT(i), nullptr);
        }
    }
	// データ転送が完了したリソースを返す。呼び出し元は、このリソースを使用してSRVを作成し、テクスチャとして使用することができる。
    return resource;
}

// TextureManagerクラスのメンバー関数の実装。テクスチャの初期化、読み込み、SRVヒープの取得、GPUハンドルの取得、リソース説明の取得などを行う。
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	// DirectXCommonのポインタを保存
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    assert(dxCommon_);
    assert(srvManager_);

	// SRVヒープは SrvManager が一元管理する。TextureManager は必要な枠だけを確保して使う。
    ID3D12Device* device = dxCommon_->GetDevice();
    HRESULT hr = S_OK;

	// テクスチャデータの初期化。
    // テクスチャデータを格納するための構造体を初期化する。
    // ここでは、テクスチャリソース、アップロード用の中間リソース、
    // SRVのCPUハンドルとGPUハンドル、
    // リソースの説明などを格納するための構造体を定義している。
    // 1x1の白テクスチャをデフォルト（インデックス0）として作成し、未初期化アクセスを防ぐ
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_CUSTOM, D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 1, 1 };
    
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultTexture;
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&defaultTexture)
    );
    assert(SUCCEEDED(hr));

    // データを書き込む (1ピクセル分、白色 0xFFFFFFFF)
    uint32_t color = 0xFFFFFFFF; // RGBA(255, 255, 255, 255)
    hr = defaultTexture->WriteToSubresource(0, nullptr, &color, sizeof(uint32_t), sizeof(uint32_t));
    assert(SUCCEEDED(hr));

    uint32_t index = srvManager_->Allocate();
    assert(index == 0);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvManager_->CreateSRV(index, defaultTexture.Get(), srvDesc);

    TextureData defaultData;
    defaultData.resource = defaultTexture;
    defaultData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(index);
    defaultData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(index);
    defaultData.resourceDesc = desc;

    textures_.push_back(defaultData);
}

// テクスチャの読み込み関数。指定されたファイルパスからテクスチャを読み込み、SRVを作成して管理する。既に同じファイルパスのテクスチャが読み込まれている場合は、そのテクスチャのハンドルを返す。
uint32_t TextureManager::LoadTexture(const std::string& filePath) {
    // 1. 既に読み込み済みかチェック
    if (fileMap_.contains(filePath)) {
        return fileMap_[filePath];
    }
	// 新規読み込みの場合は、テクスチャ数の上限をチェックしてから読み込む。上限を超える場合は、assertで強制停止させる。
    assert(textures_.size() < kMaxTextures);


    // 2. ファイル読み込み (DirectXTex)
    DirectX::ScratchImage image;

	// 文字列をUTF-8からUTF-16に変換して、DirectXTexのLoadFromWICFile関数に渡す。LoadFromWICFile関数は、指定されたファイルパスからテクスチャを読み込み、
    // ScratchImage構造体に格納する。
    // WIC_FLAGS_FORCE_SRGBフラグを指定して、sRGBカラースペースで読み込むようにしている。
    std::wstring wFilePath = ConvertString(filePath);

    HRESULT hr;
    if (wFilePath.ends_with(L".dds") || wFilePath.ends_with(L".DDS")) {
        hr = DirectX::LoadFromDDSFile(wFilePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
    } else {
        hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    }
    
    // ★ここで失敗を検知する。アサートではなく警告ログ＋フォールバックで続行する
    if (FAILED(hr)) {
        std::string message = "[TextureManager] WARNING: Failed to load texture (file not found or unsupported format): " + filePath + "\n";
        OutputDebugStringA(message.c_str());
        // ハンドル0（ダミー/白テクスチャ）を返してクラッシュを防ぐ
        return 0;
    }

    DirectX::TexMetadata originalMetadata = image.GetMetadata();

    // 3. ミップマップ生成
    DirectX::ScratchImage mipImages;
    if (DirectX::IsCompressed(originalMetadata.format) ||
        originalMetadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE ||
        originalMetadata.arraySize == 6) {
        // CubeMapはGenerateMipMapsせず、そのまま使う
        mipImages = std::move(image);
    } else {
        hr = DirectX::GenerateMipMaps(
            image.GetImages(),
            image.GetImageCount(),
            image.GetMetadata(),
            DirectX::TEX_FILTER_SRGB,
            0,
            mipImages
        );
        assert(SUCCEEDED(hr));
    }


    // 4. テクスチャリソースの作成
	// ミップマップのメタデータからリソースの説明を作成する。
    // DirectXTexのScratchImageからテクスチャのメタデータを取得し、
    // D3D12_RESOURCE_DESC構造体を初期化して、
    // テクスチャの幅、高さ、ミップレベル数、フォーマットなどを設定する。
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Width = UINT(metadata.width);
    textureDesc.Height = UINT(metadata.height);
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    textureDesc.Format = metadata.format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	// D3D12_HEAP_PROPERTIES構造体を初期化して、
    // テクスチャリソースを作成するためのヒーププロパティを設定する。
    // テクスチャリソースは、GPUがアクセスするためのリソースであり、
    // D3D12_HEAP_TYPE_DEFAULTを指定して、GPU専用のヒープに配置されるようにしている。
    D3D12_HEAP_PROPERTIES textureHeapProps = {};
    textureHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &textureHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // データ転送前はコピー先状態にしておく
        nullptr,
        IID_PPV_ARGS(&textureResource));
    assert(SUCCEEDED(hr));

    // 5. 中間リソース（Upload Heap）の作成
    // データ転送に必要なサイズやレイアウトを計算
    UINT64 subresourceCount = metadata.mipLevels * metadata.arraySize;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subresourceCount);
    UINT64 uploadBufferSize = 0;
    dxCommon_->GetDevice()->GetCopyableFootprints(&textureDesc, 0, UINT(subresourceCount), 0, layouts.data(), nullptr, nullptr, &uploadBufferSize);

	// CPUアクセス可能なアップロードヒープを作成。
    // D3D12_HEAP_TYPE_UPLOADを指定して、CPUから書き込み可能で、
    // GPUからは読み取り専用のリソースを作成するためのヒーププロパティを設定する。
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;


	// D3D12_RESOURCE_DESC構造体を初期化して、
    // アップロード用のバッファの説明を設定する。
    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Alignment = 0;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// アップロード用の中間リソースを作成。
    // CreateCommittedResource関数を使用して、
    // アップロード用のバッファを作成する。
    // ヒーププロパティ、リソースの説明、初期状態などを指定する。
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&intermediateResource)
    );
    assert(SUCCEEDED(hr));

    // 6. データ転送（CPU -> Upload Heap）
    uint8_t* pData = nullptr;
    hr = intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    assert(SUCCEEDED(hr));

	// 配列（Cubeの各面など）とミップマップごとにデータを転送するためのループ。
    for (size_t arrayIdx = 0; arrayIdx < metadata.arraySize; ++arrayIdx) {
        for (size_t mipIdx = 0; mipIdx < metadata.mipLevels; ++mipIdx) {
            size_t subresourceIdx = arrayIdx * metadata.mipLevels + mipIdx;
            const DirectX::Image* img = mipImages.GetImage(mipIdx, arrayIdx, 0);
            assert(img);

            // 書き込み先のポインタ計算（レイアウトのオフセットを加算）
            uint8_t* dstStart = pData + layouts[subresourceIdx].Offset;
            const uint8_t* srcStart = img->pixels;

            // 圧縮フォーマットと非圧縮フォーマットでコピーする行数を変更
            size_t numRows = 0;
            if (DirectX::IsCompressed(metadata.format)) {
                // BC圧縮などの場合、ブロック行数を計算 (1ブロックは4x4ピクセル)
                numRows = (img->height + 3) / 4;
            } else {
                numRows = img->height;
            }

            // 行ごとにコピー（アライメント対応）
            for (size_t y = 0; y < numRows; ++y) {
                memcpy(
                    dstStart + y * layouts[subresourceIdx].Footprint.RowPitch,
                    srcStart + y * img->rowPitch,
                    img->rowPitch
                );
            }
        }
    }
	// データのコピーが完了したら、リソースをアンマップする。
    intermediateResource->Unmap(0, nullptr);

    // 7. データ転送コマンド発行（Upload Heap -> Texture Resource）
    auto commandList = dxCommon_->GetCommandList();

    for (size_t arrayIdx = 0; arrayIdx < metadata.arraySize; ++arrayIdx) {
        for (size_t mipIdx = 0; mipIdx < metadata.mipLevels; ++mipIdx) {
            size_t subresourceIdx = arrayIdx * metadata.mipLevels + mipIdx;

            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = textureResource.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = UINT(subresourceIdx);

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = intermediateResource.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts[subresourceIdx];

            commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }

    // 8. リソースバリア（CopyDest -> PixelShaderResource）
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);


    // 9. 構造体に保存
    TextureData data;
    data.resource = textureResource;
    data.intermediateResource = intermediateResource;
    data.resourceDesc = textureResource->GetDesc();

    // 10. SRV作成
    uint32_t index = srvManager_->Allocate();
    assert(index == static_cast<uint32_t>(textures_.size()));

    data.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(index);
    data.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(index);

	// D3D12_SHADER_RESOURCE_VIEW_DESC構造体を初期化して、
    // SRVの説明を設定する。
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = data.resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    
    if ((metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) || metadata.arraySize == 6) {
        char buf[256];
        sprintf_s(buf, "[TextureManager] LoadTexture Cubemap: path=%s, arraySize=%d, miscFlags=%d, index=%d\n", filePath.c_str(), (int)metadata.arraySize, (int)metadata.miscFlags, (int)index);
        OutputDebugStringA(buf);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MipLevels = UINT(metadata.mipLevels);
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else {
        char buf[256];
        sprintf_s(buf, "[TextureManager] LoadTexture 2D: path=%s, arraySize=%d, miscFlags=%d, index=%d\n", filePath.c_str(), (int)metadata.arraySize, (int)metadata.miscFlags, (int)index);
        OutputDebugStringA(buf);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
    }

	// CreateShaderResourceView関数を呼び出して、
    // テクスチャリソースに対するSRVを作成する。
    // SRVは、GPUがテクスチャリソースにアクセスするためのビューであり、
    // シェーダーからテクスチャを使用するために必要である。
    srvManager_->CreateSRV(index, data.resource.Get(), srvDesc);

    textures_.push_back(data);
    fileMap_[filePath] = index;

    return index;


}

// SRVヒープのCPUハンドルを取得する関数。
// 指定されたテクスチャハンドルに対応するSRVのCPUハンドルを返す。
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureHandle) {
    if (textureHandle >= textures_.size()) {
        return {};
    }
    return textures_[textureHandle].srvHandleGPU;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleCPU(uint32_t textureHandle) const {
    if (textureHandle >= textures_.size()) {
        return {};
    }

    return textures_[textureHandle].srvHandleCPU;
}

// SRVヒープのGPUハンドルを取得する関数。
const D3D12_RESOURCE_DESC& TextureManager::GetResourceDesc(uint32_t textureHandle) {
    return textures_[textureHandle].resourceDesc;
}

ID3D12Resource* TextureManager::GetResource(uint32_t textureHandle) const {
    if (textureHandle >= textures_.size()) {
        return nullptr;
    }

    return textures_[textureHandle].resource.Get();
}

uint32_t TextureManager::RegisterExternalTexture(ID3D12Resource* resource) {
    assert(textures_.size() < kMaxTextures);

    // 1. 新しい空きスロットを確保
    uint32_t index = srvManager_->Allocate();
    assert(index == static_cast<uint32_t>(textures_.size()));
    TextureData data;
    data.resource = resource;
    data.resourceDesc = resource->GetDesc();

    // 2. SrvManager が管理する共通ヒープから、この外部リソース用の住所を取得
    data.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(index);
    data.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(index);

    // 3. 「このリソースを読み取り用として使うよ」というカードを作成してヒープに登録
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    // 影用テクスチャは R32_TYPELESS なので、SRVでは R32_FLOAT として解釈させる
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvManager_->CreateSRV(index, resource, srvDesc);

    // 4. リストに追加してインデックス（ハンドル）を返す
    textures_.push_back(data);
    return index;
}

