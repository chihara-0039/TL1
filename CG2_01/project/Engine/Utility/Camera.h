#pragma once
#include "MyMath.h"
#include "Object3d.h"
#include <Object3dCommon.h>
#include <Model.h>

#include <Windows.h>
#include <memory>

class Input;

// ==============================================================
//  Camera
//
//  シーンを映す「目」に相当するクラス。
//  内部で View 行列と Projection 行列を計算・保持する。
//
//  ─── 2種類のカメラ操作モード ──────────────────────────────
//
//  1. Blender スタイル操作 (UpdateBlenderStyle)
//     ・中クリック ドラッグ → 軌道 (Orbit)
//     ・Shift + 中クリック ドラッグ → パン (Pan)
//     ・ホイール → ズーム
//     ・スキニングエディタ・ステージエディタで使用
//
//  2. ゲームプレイカメラ (GameplayCameraController から制御)
//     ・プレイヤーを追従する三人称視点
//     ・GameplayCameraController::Update() から SetPosition/SetTarget 等を呼ぶ
//
//  ─── 行列の計算順序 ──────────────────────────────────────
//  Update() を呼ぶと内部で以下を計算する:
//    viewMatrix_       = LookAt(position, target, up)
//                        ※ target_ と distance_ から position を自動計算
//    projectionMatrix_ = Perspective(fov, aspect, near, far)
//
//  計算後は GetViewMatrix() / GetProjectionMatrix() で取得し、
//  Object3d::SetCamera() などに渡す。
// ==============================================================
class Camera {
public:
    Camera();

    // -------------------------------------------------------
    //  Update : ビュー行列とプロジェクション行列を再計算する。
    //  毎フレーム必ず呼ぶこと。SetPosition / SetRotation などで
    //  パラメータを変更した後、このメソッドで行列を確定させる。
    // -------------------------------------------------------
    void Update();

    // ── ゲッター ─────────────────────────────────────────

    /// <summary>
    /// ビュー行列 (World → Camera 空間への変換行列)。
    /// Object3d::SetCamera() や ShadowMap の lightVP などに渡す。
    /// </summary>
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    /// <summary>
    /// プロジェクション行列 (Camera → クリップ空間への変換行列)。
    /// FOV・アスペクト比・Near/Far クリップを元に計算される。
    /// </summary>
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

    /// <summary>ワールド空間でのカメラ位置 (スペキュラー計算などに使用)</summary>
    const Vector3& GetPosition() const { return transform_.translate; }

    /// <summary>カメラのオイラー角回転 (ラジアン)</summary>
    const Vector3& GetRotation() const { return transform_.rotate; }

    // ── セッター ─────────────────────────────────────────

    /// <summary>カメラのワールド位置を直接セット</summary>
    void SetPosition(const Vector3& pos) { transform_.translate = pos; }

    /// <summary>カメラの回転をオイラー角 (ラジアン) でセット</summary>
    void SetRotation(const Vector3& rot) { transform_.rotate = rot; }

    /// <summary>垂直方向の視野角 (Field of View) をラジアンでセット。デフォルト約 45°</summary>
    void SetFov(float fov) { fov_ = fov; }

    /// <summary>
    /// カメラが注視する点 (Orbit の中心)。
    /// distance_ と組み合わせてカメラ位置を自動計算する。
    /// </summary>
    void SetTarget(const Vector3& target) { target_ = target; }

    /// <summary>注視点 (target_) からのカメラ距離 (ズーム量)</summary>
    void SetDistance(float distance) { distance_ = distance; }

    /// <summary>
    /// アスペクト比 (幅 / 高さ)。
    /// Debug ビルドではビューポートが左 320px 分狭いため 1280/720 を手動でセットする。
    /// </summary>
    void SetAspectRatio(float aspect) { aspectRatio_ = aspect; }

    // -------------------------------------------------------
    //  ForceReset : カメラを指定状態に即座にリセットする。
    //  スキニングエディタに切り替えた瞬間など、
    //  カメラをモデル正面に強制的に戻したいときに使う。
    // -------------------------------------------------------
    void ForceReset(const Vector3& target, float distance, const Vector3& rotation);

    // -------------------------------------------------------
    //  UpdateBlenderStyle : Blender 風カメラ操作の更新。
    //  Input から中クリック・ホイールを読み取り、
    //  target_ / distance_ / transform_.rotate を更新する。
    //  isGuiCaptured が true のとき (ImGui 上でクリック中) は
    //  カメラ操作を無効化してウィンドウ操作を優先する。
    // -------------------------------------------------------
    void UpdateBlenderStyle(
        const class Input* input,
        bool isGuiCaptured,
        HWND hwnd,
        bool invertOrbit = false);

    // -------------------------------------------------------
    //  GetTransform : Transform 構造体への参照を返す。
    //  ImGui から直接 position / rotation を操作するために公開している。
    // -------------------------------------------------------
    Transform& GetTransform() { return transform_; }

    /// <summary>ImGui の SliderFloat などに直接渡せる FOV のポインタ</summary>
    float* GetFovPtr() { return &fov_; }

    /// <summary>ImGui にカメラパラメータを表示・編集するウィジェットを描画する</summary>
    void DrawImGui();

private:
    // ── カメラのトランスフォーム ───────────────────────────
    Transform transform_;        // position / rotation / scale (scale は未使用)

    // ── プロジェクション設定 ──────────────────────────────
    float fov_         = 0.785f; // 垂直 FOV ≒ 45° (π/4 rad)
    float aspectRatio_ = 16.0f / 9.0f; // 画面の縦横比
    float nearClip_    = 0.1f;   // これより手前は描画しない (Near クリップ面)
    float farClip_     = 1000.0f;// これより奥は描画しない (Far クリップ面)

    // ── 計算済み行列 ──────────────────────────────────────
    // Update() のたびに再計算される。毎フレーム GetXxxMatrix() で取得すること。
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // ── Orbit カメラ制御用 ────────────────────────────────
    // カメラは target_ を中心に distance_ 離れた球面上に配置される。
    // transform_.rotate でカメラの仰角・水平角を制御する。
    Vector3 target_   = { 8.0f, 0.0f, 8.0f }; // 注視点 (ステージ中心付近がデフォルト)
    float   distance_ = 20.0f;                 // 注視点からのカメラ距離
};
