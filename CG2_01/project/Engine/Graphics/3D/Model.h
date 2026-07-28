#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MyMath.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// ==============================================================
//  ModelVertexData 構造体
//
//  GPU の頂点バッファに格納する 1 頂点のデータ。
//  HLSL の InputLayout と完全に一致させる必要がある。
//
//  【各フィールド】
//  position  : ローカル座標 (x,y,z,w)。w は常に 1.0f。
//              4成分なのは HLSL の float4 と合わせるため。
//  texcoord  : UV 座標 (u,v)。0.0〜1.0 の範囲がテクスチャ 1 枚分。
//  normal    : 法線ベクトル (x,y,z)。ライティング計算に使用。
//              正規化されている必要がある。
// ==============================================================
struct ModelVertexData {
    Vector4 position; // ローカル座標 (xyz + w=1.0)
    Vector2 texcoord; // UV 座標 (0.0〜1.0)
    Vector3 normal;   // 法線ベクトル (正規化済み)
};

// ==============================================================
//  Model
//
//  OBJ ファイルまたは頂点配列からメッシュデータを読み込み、
//  GPU の頂点バッファとして保持するクラス。
//
//  ─── Object3d との関係 ────────────────────────────────────
//  Model   = メッシュデータ本体 (頂点バッファ / テクスチャ)。重い。
//  Object3d = 配置情報 (位置・回転・色)。Model を参照するだけ。
//
//  同じ Model を複数の Object3d で参照することで
//  メモリとドローコールを節約できる (インスタンシングへの布石)。
//
//  ─── 2種類の初期化方法 ──────────────────────────────────
//  1. CreateFromOBJ / Initialize : OBJ ファイルから読み込み (静的モデル)
//  2. InitializeFromVertices     : 頂点配列から動的に生成 (スキニングなど)
//
//  ─── 頂点バッファの仕組み ────────────────────────────────
//  Initialize() でアップロードヒープ上にバッファを確保し、
//  頂点データを Map() で CPU 側から書き込む。
//  Draw() では VertexBufferView をセットして DrawInstanced() を呼ぶ。
// ==============================================================
class Model {
public:
    // -------------------------------------------------------
    //  CreateFromOBJ : OBJ ファイルからモデルを生成するファクトリ関数。
    //  directoryPath : ファイルが置かれているディレクトリ (例: "Resources/Models/block")
    //  filename      : OBJ ファイル名 (例: "block.obj")
    //  戻り値        : 初期化済み Model の unique_ptr
    //  ※ テクスチャは OBJ に紐づく MTL ファイルから自動的に読み込まれる。
    // -------------------------------------------------------
    static std::unique_ptr<Model> CreateFromOBJ(
        DirectXCommon*     dxCommon,
        const std::string& directoryPath,
        const std::string& filename,
        TextureManager*    textureManager);

    // -------------------------------------------------------
    //  Initialize : OBJ ファイルを読み込んで GPU バッファを生成する。
    //  CreateFromOBJ() の内部で呼ばれる。
    // -------------------------------------------------------
    void Initialize(
        DirectXCommon*     dxCommon,
        const std::string& directoryPath,
        const std::string& filename,
        TextureManager*    textureManager);

    // -------------------------------------------------------
    //  InitializeFromVertices : 頂点配列から GPU バッファを生成する。
    //  OBJ ファイルを使わずに動的にメッシュを作りたい場合に使用。
    //  例: スキニングで CPU 側でスキニング計算した後の頂点を渡す。
    // -------------------------------------------------------
    void InitializeFromVertices(
        DirectXCommon*                    dxCommon,
        const std::vector<ModelVertexData>& vertices,
        uint32_t                          textureHandle);

    // -------------------------------------------------------
    //  UpdateVertexBuffer : 頂点データを GPU バッファに書き直す。
    //  スキニングのようにフレームごとに頂点が変わる場合に使用。
    //  InitializeFromVertices() で作ったバッファにのみ有効。
    // -------------------------------------------------------
    void UpdateVertexBuffer(const std::vector<ModelVertexData>& vertices);

    // -------------------------------------------------------
    //  Draw : 頂点バッファをセットして描画コマンドを積む。
    //  Object3dCommon::PreDraw() と Object3d の定数バッファバインドが
    //  事前に完了していることを前提とする。
    // -------------------------------------------------------
    void Draw(ID3D12GraphicsCommandList* commandList);

    // -------------------------------------------------------
    //  DrawInstanced : インスタンシング描画用。
    //  instanceCount 分だけ同一メッシュを 1 ドローコールで描く。
    //  インスタンスごとの変換行列は StructuredBuffer で渡す必要がある。
    // -------------------------------------------------------
    void DrawInstanced(ID3D12GraphicsCommandList* commandList, UINT instanceCount);

    // ── ゲッター ─────────────────────────────────────────

    /// <summary>テクスチャの SRV ハンドル (TextureManager が管理するインデックス)</summary>
    uint32_t GetTextureHandle() const { return textureHandle_; }

    /// <summary>頂点数 (デバッグ・確認用)</summary>
    size_t GetVertexCount() const { return vertices_.size(); }

    /// <summary>インデックス数 (デバッグ・確認用)</summary>
    size_t GetIndexCount() const { return indices_.size(); }

private:
    // ── OBJ/MTL ファイルの読み込み ─────────────────────────

    /// <summary>OBJ ファイルを解析して vertices_ に頂点データを格納する</summary>
    void LoadObjFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>MTL ファイルを解析してテクスチャパスを取得する</summary>
    void LoadMaterialFile(const std::string& directoryPath, const std::string& filename);

    /// <summary>vertices_ の内容を GPU 頂点バッファに転送する</summary>
    void CreateBuffers(DirectXCommon* dxCommon);

    // ── デフォルトテクスチャパス ─────────────────────────
    std::string textureFilePath_ = "Resources/uvChecker.png";

private:
    // ── メッシュデータ (CPU 側) ────────────────────────────
    std::vector<ModelVertexData> vertices_; // 頂点の配列 (OBJ 解析結果)
    std::vector<uint32_t> indices_;         // インデックスの配列 (OBJ 解析結果)
    uint32_t textureHandle_ = 0;            // TextureManager に登録されたテクスチャの ID

    // ── GPU リソース ──────────────────────────────────────
    // アップロードヒープ上に確保された頂点バッファ。
    // Map したまま保持することで CPU からの書き換えを高速化している。
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW               vertexBufferView_{}; // バッファの場所・サイズ・ストライドを GPU に教えるビュー

    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_INDEX_BUFFER_VIEW                indexBufferView_{};
};
