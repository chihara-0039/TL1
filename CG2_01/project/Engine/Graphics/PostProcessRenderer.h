#pragma once
#include "DirectXCommon.h"
#include "MyMath.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>

/// <summary>
/// オフスクリーンレンダリングとポストプロセスエフェクトを管理するクラス。
///
/// 役割：
///   - RenderTexture (1280x720) の生成・管理
///   - RTV / SRV デスクリプタヒープの管理
///   - コピー用 RootSignature と PSO 群 (Normal / Grayscale / Sepia / Vignette) の管理
///   - オフスクリーン描画開始 (BeginRender) / 終了 (EndRender) / コピー (DrawToBackBuffer) の実行
///   - ImGui による設定UIの描画
///
/// MyGame はこのクラスへ委譲することで、オフスクリーン関連のメンバーを持たずに済む。
/// </summary>
class PostProcessRenderer {
public:
    // ========== 構造体 ==========

    /// <summary>ヴィネッティング用の定数バッファ構造体 (GPU 側と16バイト単位でアライン必須)</summary>
    struct VignetteParams {
        float scale;      ///< ヴィネットの効果半径スケール (大きいほど効果が外側に留まる)
        float exponent;   ///< ヴィネットの輝度減衰指数 (大きいほど急激に暗くなる)
        float padding[2]; ///< 16バイトアライメントのためのパディング (使用しない)
    };

    struct OutlineParams {
        Matrix4x4 projectionInverse;
        float depthStrength;
        float padding[3];
    };

    struct RadialBlurParams {
        Vector2 center;
        float blurWidth;
        int32_t sampleCount;
    };

    struct DissolveParams {
        float threshold;
        float edgeWidth;
        int32_t maskIndex;
        float padding;
        Vector4 edgeColor;
    };

    struct RandomParams {
        float time;
        int32_t mode;
        float strength;
        float padding;
    };

    PostProcessRenderer() = default;
    ~PostProcessRenderer() = default;

    // ========== 初期化 ==========

    /// <summary>
    /// 全リソースの初期化。
    /// RenderTexture・RTV/SRV ヒープ・RootSignature・PSO群・定数バッファを生成する。
    /// </summary>
    /// <param name="dxCommon">DirectXCommon へのポインタ (デバイス・シェーダーコンパイル用)</param>
    /// <param name="clearColor">初期のクリアカラー (RGBA)</param>
    void Initialize(DirectXCommon* dxCommon, const Vector4& clearColor);

    // ========== 描画フロー ==========

    /// <summary>
    /// オフスクリーン描画の開始。
    /// RenderTexture を RENDER_TARGET 状態へ遷移させ、RTVをセットし、クリアする。
    /// この後、通常の RenderScene() を呼び出すことで RenderTexture に描き込む。
    /// </summary>
    void BeginRender(ID3D12GraphicsCommandList* cmdList, DirectXCommon* dxCommon);

    /// <summary>
    /// オフスクリーン描画の終了。
    /// RenderTexture を PIXEL_SHADER_RESOURCE 状態へ遷移させる。
    /// BeginRender の後、RenderScene の後に呼ぶこと。
    /// </summary>
    void EndRender(ID3D12GraphicsCommandList* cmdList);

    /// <summary>
    /// バックバッファへのポストエフェクト適用コピー描画。
    /// postEffectMode_ に応じた PSO を選択し、画面全体をカバーする三角形を描画する。
    /// dxCommon->PreDraw() 後 (バックバッファが RT になった後) に呼ぶこと。
    /// </summary>
    void DrawToBackBuffer(ID3D12GraphicsCommandList* cmdList, const Matrix4x4& projectionMatrix);

    /// <summary>ImGui による設定パネルの描画 (CollapsingHeader 内に収まる形式)</summary>
    void DrawImGui();

    // ========== ゲッター / セッター ==========

    /// <summary>オフスクリーンレンダリングが有効かどうか</summary>
    bool IsEnabled() const { return enabled_; }

    /// <summary>オフスクリーンレンダリングの有効/無効を設定する</summary>
    void SetEnabled(bool v) { enabled_ = v; }

    /// <summary>現在のクリアカラーを返す</summary>
    const Vector4& GetClearColor() const { return clearColor_; }

    /// <summary>クリアカラーを設定する (RenderTexture クリア時に使用)</summary>
    void SetClearColor(const Vector4& c) { clearColor_ = c; }

    /// <summary>スカイボックス連動モードを返す (0: なし / 1: Link-Multiply)</summary>
    int GetSkyboxLinkMode() const { return skyboxLinkMode_; }

    /// <summary>ポストエフェクトモードを返す (0: Normal / 1: Grayscale / 2: Sepia / 3: Vignette / 4: BoxFilter3x3 / 5: BoxFilter5x5)</summary>
    int GetPostEffectMode() const { return postEffectMode_; }

    /// <summary>Sets the active post effect. Values outside the supported range fall back to Normal.</summary>
    void SetPostEffectMode(int mode) { postEffectMode_ = (mode >= 0 && mode <= 11) ? mode : 0; }

    /// <summary>Sets the dissolve cut amount used by the Release showcase and ImGui.</summary>
    void SetDissolveThreshold(float threshold) {
        if (!dissolveParamsData_) {
            return;
        }
        if (threshold < 0.0f) {
            threshold = 0.0f;
        }
        if (threshold > 1.0f) {
            threshold = 1.0f;
        }
        dissolveParamsData_->threshold = threshold;
    }

    /// <summary>Sets the random effect mode. Values outside the shader's range use grayscale noise.</summary>
    void SetRandomMode(int mode) {
        if (!randomParamsData_) {
            return;
        }
        randomParamsData_->mode = (mode >= 0 && mode <= 1) ? mode : 0;
    }

    /// <summary>Sets the random effect strength used by the Release showcase and ImGui.</summary>
    void SetRandomStrength(float strength) {
        if (!randomParamsData_) {
            return;
        }
        if (strength < 0.0f) {
            strength = 0.0f;
        }
        if (strength > 1.0f) {
            strength = 1.0f;
        }
        randomParamsData_->strength = strength;
    }

private:
    // ========== 内部ヘルパー ==========

    /// <summary>
    /// D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET 付きの Texture2D リソースを生成するヘルパー。
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor);

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResourceFromFile(
        ID3D12Device* device,
        const wchar_t* filePath);

private:
    // ========== DirectX12 リソース ==========

    Microsoft::WRL::ComPtr<ID3D12Resource>       renderTexture_;          ///< オフスクリーン用レンダーテクスチャ (1280x720)
    Microsoft::WRL::ComPtr<ID3D12Resource>       dissolveMaskTextures_[2];
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;               ///< RTV ヒープ (RenderTexture を RT として使うため)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;               ///< SRV ヒープ (コピーパスでシェーダーから参照するため SHADER_VISIBLE)
    ID3D12Resource*                               depthStencilResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  copyRootSignature_;      ///< コピー/エフェクト用 RootSignature (SRV t0, CBV b0)
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  copyPipelineState_;      ///< 通常コピー PSO (エフェクトなし)
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  grayscalePipelineState_; ///< グレースケール PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  sepiaPipelineState_;     ///< セピア調 PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  vignettePipelineState_;  ///< ヴィネッティング PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  boxFilter3x3PipelineState_; ///< 3x3 BoxFilter PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  boxFilter5x5PipelineState_; ///< 5x5 BoxFilter PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  gaussianFilterPipelineState_; ///< GaussianFilter PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  luminanceOutlinePipelineState_; ///< LuminanceBasedOutline PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  depthOutlinePipelineState_; ///< DepthBasedOutline PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  radialBlurPipelineState_; ///< RadialBlur PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  dissolvePipelineState_; ///< Dissolve PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  randomPipelineState_; ///< Random PSO
    Microsoft::WRL::ComPtr<ID3D12Resource>       vignetteConstantBuffer_; ///< ヴィネット用定数バッファ (Upload ヒープ)
    Microsoft::WRL::ComPtr<ID3D12Resource>       outlineConstantBuffer_; ///< Outline用定数バッファ (Upload ヒープ)
    Microsoft::WRL::ComPtr<ID3D12Resource>       radialBlurConstantBuffer_; ///< RadialBlur用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource>       dissolveConstantBuffer_; ///< Dissolve用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource>       randomConstantBuffer_; ///< Random用定数バッファ
    VignetteParams*                              vignetteParamsData_ = nullptr; ///< 定数バッファのマップ済みポインタ
    OutlineParams*                               outlineParamsData_ = nullptr;
    RadialBlurParams*                            radialBlurParamsData_ = nullptr;
    DissolveParams*                              dissolveParamsData_ = nullptr;
    RandomParams*                                randomParamsData_ = nullptr;

    /// <summary>
    /// renderTexture_ の現在のリソース状態 (遷移前後の整合を取るために保持)。
    /// </summary>
    D3D12_RESOURCE_STATES renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // ========== 設定パラメータ ==========

    bool    enabled_        = false;                       ///< オフスクリーン有効フラグ (false 時は通常バックバッファへ直接描画)
    Vector4 clearColor_     = { 0.1f, 0.1f, 0.1f, 1.0f }; ///< RenderTexture のクリアカラー (初期値: ほぼ黒)
    int     postEffectMode_ = 0; ///< ポストエフェクトモード (0:Normal / 1:Grayscale / 2:Sepia / 3:Vignette / 4:BoxFilter3x3 / 5:BoxFilter5x5)
    int     skyboxLinkMode_ = 0; ///< スカイボックス色連動モード (0:なし / 1:Link-Multiply)
};
