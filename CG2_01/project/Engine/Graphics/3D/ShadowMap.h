#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>

// ==============================================================
//  ShadowMap
//
//  シャドウマッピング (影生成) を管理するクラス。
//
//  ─── シャドウマッピングとは ──────────────────────────────
//  「ライトカメラ視点でシーンを深度バッファに描画し、
//   本カメラ描画時に各ピクセルがライトから見えるかを判定する」技法。
//
//  影が表示される手順:
//    1. PreDraw()    … DSV をクリアし、深度値のみ書き込む準備
//    2. (影を落とすオブジェクトを DrawShadow() で描画)
//    3. PostDraw()  … SRV としてシェーダーが読める状態にバリア遷移
//    4. 本描画時に GetSrvHandle() を RootDescriptorTable にバインド
//    5. ピクセルシェーダー内で shadowMap.Sample() して影か否かを判定
//
//  ─── 解像度について ──────────────────────────────────────
//  kWidth = kHeight = 4096 (高解像度)。
//  高いほど影のジャギー (ギザギザ) が減るが VRAM を多く使う。
//  低スペック PC では 2048 に下げると軽くなる。
//
//  ─── ビューの種類 ────────────────────────────────────────
//  DSV (Depth Stencil View) : 影描画パスでの書き込み口。
//  SRV (Shader Resource View) : 本描画パスでのシェーダー読み込み口。
//  同一リソースを DSV と SRV 両方で使うため、
//  リソースバリアで「書き込みモード↔読み込みモード」を切り替える。
// ==============================================================
class ShadowMap {
public:
    /// <summary>シャドウマップの幅 (ピクセル)。高いほど影が綺麗になる。</summary>
    static const int kWidth  = 4096;
    /// <summary>シャドウマップの高さ (ピクセル)。kWidth と同値を推奨。</summary>
    static const int kHeight = 4096;

    // -------------------------------------------------------
    //  Initialize : シャドウマップ用の深度テクスチャとビューを生成する。
    //  textureManager には SRV を登録し、GetSrvHandle() で取得できるようにする。
    // -------------------------------------------------------
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // -------------------------------------------------------
    //  PreDraw : 影描画パスの開始。
    //  ・リソースバリアを SRV 読み取り → DSV 書き込みに遷移
    //  ・DSV をクリア (深度値を 1.0 にリセット)
    //  ・ビューポートとシザー矩形を kWidth × kHeight にセット
    //  ・CommandList に DSV をバインド (RTV なし = カラー書き込みなし)
    //
    //  この後に DrawShadow() を呼び、PostDraw() で終了する。
    // -------------------------------------------------------
    void PreDraw(ID3D12GraphicsCommandList* commandList);

    // -------------------------------------------------------
    //  PostDraw : 影描画パスの終了。
    //  ・リソースバリアを DSV 書き込み → SRV 読み取りに遷移
    //  本描画パスで GetSrvHandle() をシェーダーに渡せる状態になる。
    // -------------------------------------------------------
    void PostDraw(ID3D12GraphicsCommandList* commandList);

    // ── ゲッター ─────────────────────────────────────────

    /// <summary>深度テクスチャリソース本体 (デバッグ・詳細設定用)</summary>
    ID3D12Resource* GetResource() const { return resource_.Get(); }

    /// <summary>
    /// DSV の CPU ハンドル。PreDraw() 内で DSV をセットするために使用。
    /// DSV ヒープに格納されている「書き込み口」へのアドレス。
    /// </summary>
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const { return dsvHandle_; }

    /// <summary>
    /// SRV の GPU ハンドル。本描画時に RootDescriptorTable にバインドするために使用。
    /// commandList->SetGraphicsRootDescriptorTable(スロット番号, GetSrvHandle()) で渡す。
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return srvHandle_; }

private:
    // ── GPU リソース ──────────────────────────────────────

    /// <summary>深度テクスチャリソース (DSV と SRV 両方で使い回す)</summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

    /// <summary>DSV の CPU ハンドル (影描画パスで書き込む際に使用)</summary>
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

    /// <summary>SRV の GPU ハンドル (本描画パスでシェーダーから参照する際に使用)</summary>
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_{};

    /// <summary>DSV 専用のディスクリプタヒープ (DSV だけを格納する)</summary>
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_{};
};

