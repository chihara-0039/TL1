#include "GameplayCameraController.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void GameplayCameraController::Initialize() {
    cameraAngle_ = 6.267f;
    cameraPitch_ = 0.400f;
    cameraFov_ = 0.55f;

    cameraPivot_ = { 4.0f, 9.0f, 4.5f };
    cameraDistance_ = 35.0f;
    cameraHeight_ = 20.0f;

    followPlayerMode_ = true;
    cameraDirty_ = true;
}

void GameplayCameraController::Update(Input* input, Camera* camera, WinApp* winApp, Player* player) {
    if (!input || !camera || !winApp || !player) {
        return;
    }

    const auto& mouse = input->GetMouseState();

    bool isGuiCaptured = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (ImGui::GetCurrentContext()) {
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
    }
#endif

    bool hasCameraParameterChanged = false;

    if (followPlayerMode_) {
        // プレイヤー追従時は、上半身付近をカメラの注視点にする。
        Vector3 targetPivot = player->GetPosition();
        targetPivot.y += 0.8f;

        if (std::abs(cameraPivot_.x - targetPivot.x) > 0.001f ||
            std::abs(cameraPivot_.y - targetPivot.y) > 0.001f ||
            std::abs(cameraPivot_.z - targetPivot.z) > 0.001f) {
            cameraPivot_ = targetPivot;
            hasCameraParameterChanged = true;
        }
    }

    if (!isGuiCaptured && mouse.wheel != 0) {
        float minFov = minFov_;
        float maxFov = maxFov_;

        if (currentStageIndex_ == 3) {
            minFov = 0.25f;
            maxFov = 0.80f;
        }

        const float zoomStep = (maxFov - minFov) / 5.0f;
        if (mouse.wheel > 0) {
            cameraFov_ -= zoomStep;
        } else if (mouse.wheel < 0) {
            cameraFov_ += zoomStep;
        }

        cameraFov_ = std::clamp(cameraFov_, minFov, maxFov);
        camera->SetFov(cameraFov_);
        hasCameraParameterChanged = true;
    }

    // 画面端のガイド領域を左クリックした時だけ、カメラを段階的に回転させる。
    if (mouse.buttons[0] && !isGuiCaptured) {
        RECT clientRect;
        GetClientRect(winApp->GetHwnd(), &clientRect);

        float currentClientWidth = static_cast<float>(clientRect.right - clientRect.left);
        float currentClientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

        if (currentClientWidth > 0.0f && currentClientHeight > 0.0f) {
            float windowScaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientWidth;
            float windowScaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientHeight;
            float scaledMouseX = static_cast<float>(mouse.posX) * windowScaleX;
            float scaledMouseY = static_cast<float>(mouse.posY) * windowScaleY;

#ifdef NDEBUG
            float mouseX = static_cast<float>(mouse.posX) *
                (static_cast<float>(WinApp::kClientWidth) / currentClientWidth);

            float mouseY = static_cast<float>(mouse.posY) *
                (static_cast<float>(WinApp::kClientHeight) / currentClientHeight);
#else
            float debugViewportOffsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
            float mouseX = scaledMouseX - debugViewportOffsetX;
            float mouseY = scaledMouseY;
#endif

            float screenWidth = static_cast<float>(WinApp::kClientWidth);
            float screenHeight = static_cast<float>(WinApp::kClientHeight);

            float edgeRatio = 0.1f;
            float leftGuideX = screenWidth * edgeRatio * 0.5f;
            float rightGuideX = screenWidth * (1.0f - edgeRatio * 0.5f);
            float topGuideY = screenHeight * edgeRatio * 0.5f;
            float bottomGuideY = screenHeight * (1.0f - edgeRatio * 0.5f);

            float centerX = screenWidth * 0.5f;
            float centerY = screenHeight * 0.5f;

            const float rotateSpeed = 0.025f;
            float minPitch = 0.3f;
            float maxPitch = 1.5f;

            if (currentStageIndex_ == 3) {
                minPitch = 0.2f; // 高低差の大きいステージでは少し下まで見られるようにする。
                maxPitch = 1.5f;
            }

            const float hitSize = 90.0f;
            const float halfHitSize = hitSize * 0.5f;

#ifdef NDEBUG
            Vector2 leftGuideOffset = { 0.0f, 0.0f };
            Vector2 rightGuideOffset = { -40.0f, 0.0f };
            Vector2 upGuideOffset = { 0.0f, 0.0f };
            Vector2 downGuideOffset = { 0.0f, -60.0f };
#else
            Vector2 leftGuideOffset = { 0.0f, 0.0f };
            Vector2 rightGuideOffset = { -20.0f, 0.0f };
            Vector2 upGuideOffset = { 0.0f, 0.0f };
            Vector2 downGuideOffset = { 0.0f, -20.0f };
#endif

            auto isMouseInsideGuide = [&](float guideCenterX, float guideCenterY) {
                return mouseX >= guideCenterX - halfHitSize &&
                    mouseX <= guideCenterX + halfHitSize &&
                    mouseY >= guideCenterY - halfHitSize &&
                    mouseY <= guideCenterY + halfHitSize;
            };

            bool hitLeftGuide = isMouseInsideGuide(leftGuideX + leftGuideOffset.x, centerY + leftGuideOffset.y);
            bool hitRightGuide = isMouseInsideGuide(rightGuideX + rightGuideOffset.x, centerY + rightGuideOffset.y);
            bool hitUpGuide = isMouseInsideGuide(centerX + upGuideOffset.x, topGuideY + upGuideOffset.y);
            bool hitDownGuide = isMouseInsideGuide(centerX + downGuideOffset.x, bottomGuideY + downGuideOffset.y);

            if (hitLeftGuide) {
                cameraAngle_ -= rotateSpeed;
                hasCameraParameterChanged = true;
            } else if (hitRightGuide) {
                cameraAngle_ += rotateSpeed;
                hasCameraParameterChanged = true;
            } else if (hitUpGuide) {
                cameraPitch_ -= rotateSpeed;
                hasCameraParameterChanged = true;
            } else if (hitDownGuide) {
                cameraPitch_ += rotateSpeed;
                hasCameraParameterChanged = true;
            }

            cameraPitch_ = std::clamp(cameraPitch_, minPitch, maxPitch);
        }
    }

    // 画面上の矢印操作と同じ回転をキーボードの矢印キーでも行えるようにする。
    // LEFT/RIGHTは注視点を中心とする水平旋回、UP/DOWNは上下角度を担当する。
    const float keyRotateSpeed = 0.025f;
    if (input->PushKey(DIK_LEFT)) {
        cameraAngle_ += keyRotateSpeed;
        hasCameraParameterChanged = true;
    }
    if (input->PushKey(DIK_RIGHT)) {
        cameraAngle_ -= keyRotateSpeed;
        hasCameraParameterChanged = true;
    }
    if (input->PushKey(DIK_UP)) {
        cameraPitch_ -= keyRotateSpeed;
        hasCameraParameterChanged = true;
    }
    if (input->PushKey(DIK_DOWN)) {
        cameraPitch_ += keyRotateSpeed;
        hasCameraParameterChanged = true;
    }

    // ステージ3は地形を見下ろしやすいよう、ほかのステージより少し低い角度を許可する。
    const float keyboardMinPitch = currentStageIndex_ == 3 ? 0.2f : 0.3f;
    cameraPitch_ = std::clamp(cameraPitch_, keyboardMinPitch, 1.5f);

    // 入力や追従対象に変化がないフレームでは、三角関数計算とCameraへの反映を省く。
    if (!hasCameraParameterChanged && !cameraDirty_) {
        return;
    }

    ApplyCamera(camera);
    cameraDirty_ = false;
}

void GameplayCameraController::ApplyCamera(Camera* camera) {
    if (!camera) {
        return;
    }

    Vector3 targetPosition = cameraPivot_;

    Vector3 cameraPosition;
    cameraPosition.x = targetPosition.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * cameraDistance_;
    cameraPosition.y = targetPosition.y + std::sin(cameraPitch_) * cameraHeight_;
    cameraPosition.z = targetPosition.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * cameraDistance_;

    camera->SetPosition(cameraPosition);

    Vector3 cameraToTarget = {
        targetPosition.x - cameraPosition.x,
        targetPosition.y - cameraPosition.y,
        targetPosition.z - cameraPosition.z
    };

    float cameraYaw = std::atan2(cameraToTarget.x, cameraToTarget.z);
    float horizontalDistance = std::sqrt(cameraToTarget.x * cameraToTarget.x + cameraToTarget.z * cameraToTarget.z);
    float cameraPitch = -std::atan2(cameraToTarget.y, horizontalDistance);

    camera->SetRotation({ cameraPitch, cameraYaw, 0.0f });
}

void GameplayCameraController::ResetCamera(
    Camera* camera,
    Player* player,
    const StageMap& stageMap,
    int stageIndex
) {
    if (!camera || !player) {
        return;
    }

    followPlayerMode_ = true;
    currentStageIndex_ = stageIndex;

    float stageWidth = static_cast<float>(stageMap.GetWidth());
    float stageHeight = static_cast<float>(stageMap.GetHeight());
    float stageDepth = static_cast<float>(stageMap.GetDepth());
    float maxStageSize = (std::max)(stageWidth, stageDepth);

    // ステージごとの見やすさに合わせた初期カメラ設定。
    CameraPreset preset{};

    switch (stageIndex) {
    case 0:
        preset.enableWallTransparency = false;
        preset.wallTransparencyAlpha = 1.0f;

        // 操作説明ステージ。斜め俯瞰で全体とプレイヤーを見せる。
        preset.angle = 5.55f;
        preset.pitch = 0.78f;
        preset.distanceRate = 1.55f;
        preset.heightRate = 0.90f;
        preset.fov = 0.55f;
        preset.pivotYRate = 0.45f;
        break;

    case 1:
        preset.enableWallTransparency = true;
        preset.wallTransparencyAlpha = 0.50f;

        // 通常ステージ。壁越し表示を有効にしてプレイヤーを見失いにくくする。
        preset.angle = 0.78f;
        preset.pitch = 0.72f;
        preset.distanceRate = 1.85f;
        preset.heightRate = 1.10f;
        preset.fov = 1.0f;
        preset.pivotYRate = 0.38f;
        break;

    case 2:
        // 少し広めに見たいステージ。距離と高さを増やして全体を把握しやすくする。
        preset.angle = 0.785f;
        preset.pitch = 0.68f;
        preset.distanceRate = 2.25f;
        preset.heightRate = 1.35f;
        preset.fov = 0.55f;
        preset.pivotYRate = 0.40f;
        break;

    case 3:
        // 高低差や複雑な構造があるステージ。より高い位置から見下ろす。
        preset.angle = 0.785f;
        preset.pitch = 0.75f;
        preset.distanceRate = 2.40f;
        preset.heightRate = 1.45f;
        preset.fov = 0.60f;
        preset.pivotYRate = 0.45f;
        break;

    default:
        // 通常ステージ向けの標準カメラ設定。
        preset.angle = 0.785f;
        preset.pitch = 0.60f;
        preset.distanceRate = 2.00f;
        preset.heightRate = 1.20f;
        preset.fov = 0.50f;
        preset.pivotYRate = 0.35f;
        break;
    }

    // ステージ開始時はプレイヤーの上半身付近を注視点にする。
    Vector3 playerPivotPosition = player->GetPosition();
    playerPivotPosition.y += 0.8f;
    cameraPivot_ = playerPivotPosition;

    cameraAngle_ = preset.angle;
    cameraPitch_ = preset.pitch;
    cameraDistance_ = maxStageSize * preset.distanceRate;
    cameraHeight_ = maxStageSize * preset.heightRate;
    cameraFov_ = preset.fov;

    enableWallTransparency_ = preset.enableWallTransparency;
    wallTransparencyAlpha_ = preset.wallTransparencyAlpha;

    initialPivotYOffset_ = cameraPivot_.y - player->GetPosition().y;

    camera->SetFov(cameraFov_);
    ApplyCamera(camera);
    camera->Update();

    cameraDirty_ = false;
}
