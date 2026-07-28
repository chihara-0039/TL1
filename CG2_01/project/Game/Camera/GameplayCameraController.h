#pragma once
#include "Input.h"
#include "Camera.h"
#include "WinApp.h"
#include"Player.h"
#include "StageMap.h"

/// <summary>
/// ゲームプレイ中のカメラ操作を管理するコントローラークラス。
/// マウスの画面端クリックやキー入力に応じて極座標（角度・ピッチ）を計算し、
/// カメラの位置と回転を自動計算・反映します。
/// </summary>
class GameplayCameraController {
public:
    /// <summary>
    /// カメラの角度とピッチの初期化を行います。
    /// </summary>
    void Initialize();

    /// <summary>
    /// 毎フレームの入力情報からカメラ位置を計算・更新します。
    /// </summary>
    /// <param name="input">入力管理オブジェクト</param>
    /// <param name="camera">操作対象のカメラ</param>
    /// <param name="winApp">ウィンドウ管理オブジェクト</param>
    void Update(Input* input, Camera* camera, WinApp* winApp,Player*player);

    /// <summary>
    /// 現在のカメラ横回転角度（ラジアン）を取得します。
    /// </summary>
    float GetAngle() const { return cameraAngle_; }
    void ForceUpdate() { cameraDirty_ = true; }

    /// <summary>
    /// 現在のカメラ縦見下ろし角度（ピッチ・ラジアン）を取得します。
    /// </summary>
    float GetPitch() const { return cameraPitch_; }

    /// <summary>
    /// 現在のカメラFOVを取得します。
    /// </summary>
    float GetFov() const { return cameraFov_; }

    /// <summary>
    /// カメラFOVを強制的に設定します（ImGuiデバッグ用）。
    /// </summary>
    void SetFov(float fov) { cameraFov_ = fov; }

    /// <summary>
    /// カメラの横回転角度を強制的に設定します。
    /// </summary>
    void SetAngle(float angle) { cameraAngle_ = angle; }

    /// <summary>
    /// カメラの縦見下ろし角度（ピッチ）を強制的に設定します。
    /// </summary>
    void SetPitch(float pitch) { cameraPitch_ = pitch; }

    void SetFollowPlayerMode(bool follow) { followPlayerMode_ = follow; cameraDirty_ = true; }
    bool IsFollowPlayerMode() const { return followPlayerMode_; }
    void SetCameraPivot(const Vector3& pivot) { cameraPivot_ = pivot; cameraDirty_ = true; }

    /// <summary>
    /// カメラ位置と角度をデフォルト状態にリセットし、即座に反映します。
    /// </summary>
    void ResetCamera(Camera* camera,Player*player, const StageMap& stageMap, int stageIndex);

    bool IsWallTransparencyEnabled() const { return enableWallTransparency_; }

    float GetWallTransparencyAlpha() const {
        return wallTransparencyAlpha_;
    }

private:
    void ApplyCamera(Camera* camera);
private:

    // カメラの極座標パラメータ
    float cameraAngle_ = 0.0f; // 水平方向の回転角度
    float cameraPitch_ = 0.75f; // 垂直方向の見下ろし角度


    //ズーム制限
    float minDistance_ = 18.0f;
    float maxDistance_ = 45.0f;

    //高さ倍率
    float heightRate_ = 0.55f;

    float cameraFov_ = 0.55f;
    float minFov_ = 0.25f;
    float maxFov_ = 1.0f;

    Vector3 cameraPivot_ = { 4.0f, 9.0f, 4.5f };
    float cameraDistance_ = 35.0f;
    float cameraHeight_ = 20.0f;

    bool cameraDirty_ = true;

    
    float initialPivotYOffset_ = 8.0f;
    bool followPlayerMode_ = true;

    struct CameraPreset {
        float angle;
        float pitch;
        float distanceRate;
        float heightRate;
        float fov;
        float pivotYRate;

        bool enableWallTransparency;

        float wallTransparencyAlpha;
    };

    //透過させるか
    bool enableWallTransparency_ = true;

    //透過の強さ
    float wallTransparencyAlpha_ = 0.25f;

    public:
        int currentStageIndex_ = 0;
};
