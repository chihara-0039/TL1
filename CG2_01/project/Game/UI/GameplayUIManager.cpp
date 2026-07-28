#include "GameplayUIManager.h"
#include "WinApp.h"
#include <cmath>

namespace {
// 3Dプロンプトはワールド上に置くが、常にカメラ方向へ向けることでUIとして読みやすくする。
void UpdatePromptObject3D(
    Object3d* object,
    const Vector3& promptPosition,
    Camera* camera,
    LightCamera* lightCamera)
{
    if (!object || !camera || !lightCamera) {
        return;
    }

    object->SetPosition(promptPosition);
    object->SetScale({ 0.6f, 0.6f, 0.6f });

    const Vector3 cameraPosition = camera->GetPosition();
    const float faceCameraYaw = std::atan2f(
        cameraPosition.x - promptPosition.x,
        cameraPosition.z - promptPosition.z);

    object->SetRotation({ 0.0f, faceCameraYaw, 0.0f });
    object->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    object->Update(lightCamera->GetViewProjectionMatrix());
}
}

void GameplayUIManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager, SpriteCommon* spriteCommon, Object3dCommon* object3dCommon) {
    spriteCommon_ = spriteCommon;

    // カメラ回転ガイド用の2D矢印スプライトを方向ごとに用意する。
    cameraGuideLeftTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_left.png");
    cameraGuideRightTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_right.png");
    cameraGuideUpTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_up.png");
    cameraGuideDownTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow/arrow_down.png");

    cameraGuideLeftSprite_ = std::make_unique<Sprite>();
    cameraGuideLeftSprite_->Initialize(spriteCommon, cameraGuideLeftTextureHandle_);

    cameraGuideRightSprite_ = std::make_unique<Sprite>();
    cameraGuideRightSprite_->Initialize(spriteCommon, cameraGuideRightTextureHandle_);

    cameraGuideUpSprite_ = std::make_unique<Sprite>();
    cameraGuideUpSprite_->Initialize(spriteCommon, cameraGuideUpTextureHandle_);

    cameraGuideDownSprite_ = std::make_unique<Sprite>();
    cameraGuideDownSprite_->Initialize(spriteCommon, cameraGuideDownTextureHandle_);

    // ドア、スイッチ、鍵など、プレイヤーが操作できる対象に出す「F」プロンプト。
    // 同じモデルを複数の Object3d に割り当て、表示位置だけ個別に更新する。
    doorPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon,
            "Resources/UI/F",
            "F.obj",
            textureManager));

    doorPromptObject_ = std::make_unique<Object3d>();
    doorPromptObject_->Initialize(object3dCommon);
    doorPromptObject_->SetModel(doorPromptModel_.get());
    doorPromptObject_->SetEnableLighting(false);
    doorPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    pSwitchPromptObject_ = std::make_unique<Object3d>();
    pSwitchPromptObject_->Initialize(object3dCommon);
    pSwitchPromptObject_->SetModel(doorPromptModel_.get());
    pSwitchPromptObject_->SetEnableLighting(false);
    pSwitchPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    keyPromptObject_ = std::make_unique<Object3d>();
    keyPromptObject_->Initialize(object3dCommon);
    keyPromptObject_->SetModel(doorPromptModel_.get());
    keyPromptObject_->SetEnableLighting(false);
    keyPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    keyBlockPromptObject_ = std::make_unique<Object3d>();
    keyBlockPromptObject_->Initialize(object3dCommon);
    keyBlockPromptObject_->SetModel(doorPromptModel_.get());
    keyBlockPromptObject_->SetEnableLighting(false);
    keyBlockPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // はしご専用の3Dプロンプト。Fプロンプトとは別モデルを使う。
    ladderPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon,
            "Resources/UI/radderUI",
            "radderUI.obj",
            textureManager));

    ladderPromptObject_ = std::make_unique<Object3d>();
    ladderPromptObject_->Initialize(object3dCommon);
    ladderPromptObject_->SetModel(ladderPromptModel_.get());
    ladderPromptObject_->SetEnableLighting(false);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // カメラモード表示用アイコン。俯瞰モードとプレイヤー追従モードを切り替えて描画する。
    cameraModeStageTextureHandle_ = textureManager->LoadTexture("Resources/UI/stage_overview_icon.png");
    cameraModePlayerTextureHandle_ = textureManager->LoadTexture("Resources/UI/follow_player_icon.png");

    cameraModeStageSprite_ = std::make_unique<Sprite>();
    cameraModeStageSprite_->Initialize(spriteCommon, cameraModeStageTextureHandle_);
    cameraModeStageSprite_->SetPosition({ 1180.0f, 100.0f });
    cameraModeStageSprite_->SetSize({ 64.0f, 64.0f });

    cameraModePlayerSprite_ = std::make_unique<Sprite>();
    cameraModePlayerSprite_->Initialize(spriteCommon, cameraModePlayerTextureHandle_);
    cameraModePlayerSprite_->SetPosition({ 1180.0f, 100.0f });
    cameraModePlayerSprite_->SetSize({ 64.0f, 64.0f });
}

void GameplayUIManager::Update(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    // 3Dプロンプトはプレイヤーの近接状態に依存するため、毎フレームプレイヤー情報から位置を更新する。
    UpdateDoorPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateLadderPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdatePSwitchPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateKeyPrompt3D(isGamePlayMode, player, camera, lightCamera);
    UpdateKeyBlockPrompt3D(isGamePlayMode, player, camera, lightCamera);

    if (cameraModeStageSprite_) {
        cameraModeStageSprite_->Update();
    }
    if (cameraModePlayerSprite_) {
        cameraModePlayerSprite_->Update();
    }
}

void GameplayUIManager::UpdateDoorPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    if (!doorPromptObject_ || !player) {
        return;
    }
    if (!isGamePlayMode || !player->IsNearDoor()) {
        return;
    }

    UpdatePromptObject3D(
        doorPromptObject_.get(),
        player->GetNearDoorWorldPos(),
        camera,
        lightCamera);
}

void GameplayUIManager::UpdatePSwitchPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearPSwitch()) {
        return;
    }

    UpdatePromptObject3D(
        pSwitchPromptObject_.get(),
        player->GetNearPSwitchWorldPos(),
        camera,
        lightCamera);
}

void GameplayUIManager::UpdateKeyPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearKey()) {
        return;
    }

    UpdatePromptObject3D(
        keyPromptObject_.get(),
        player->GetNearKeyWorldPos(),
        camera,
        lightCamera);
}

void GameplayUIManager::UpdateKeyBlockPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera)
{
    if (!isGamePlayMode || !player || !player->IsNearKeyBlock()) {
        return;
    }

    UpdatePromptObject3D(
        keyBlockPromptObject_.get(),
        player->GetNearKeyBlockWorldPos(),
        camera,
        lightCamera);
}

void GameplayUIManager::UpdateLadderPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera) {
    if (!ladderPromptObject_ || !player) {
        return;
    }
    if (!isGamePlayMode || !player->IsOnLadder()) {
        return;
    }

    UpdatePromptObject3D(
        ladderPromptObject_.get(),
        player->GetLadderWorldPos(),
        camera,
        lightCamera);
}

void GameplayUIManager::UpdateCameraGuide(bool isGamePlay, Input* input, WinApp* winApp)
{
    if (!isGamePlay) {
        return;
    }
    if (!input || !winApp) {
        return;
    }
    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    const auto& mouse = input->GetMouseState();

    // 矢印ガイドはゲーム画面の端に配置する。デバッグ時はImGui領域を考慮して座標補正する。
    const float screenWidth = static_cast<float>(WinApp::kClientWidth);
    const float screenHeight = static_cast<float>(WinApp::kClientHeight);
    const float edgeRatio = 0.1f;

    const float leftGuideX = screenWidth * edgeRatio * 0.5f;
    const float rightGuideX = screenWidth * (1.0f - edgeRatio * 0.5f);
    const float topGuideY = screenHeight * edgeRatio * 0.5f;
    const float bottomGuideY = screenHeight * (1.0f - edgeRatio * 0.5f);

    const float centerX = screenWidth * 0.5f;
    const float centerY = screenHeight * 0.5f;

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

    // 矢印を少し上下に揺らして、クリック可能なガイドだと分かりやすくする。
    cameraGuideTime_ += 1.0f / 60.0f;
    const float floatPower = 6.0f;
    const float floatSpeed = 3.0f;
    const float floatingOffsetY = std::sin(cameraGuideTime_ * floatSpeed) * floatPower;

    RECT clientRect;
    GetClientRect(winApp->GetHwnd(), &clientRect);

    const float currentClientWidth = static_cast<float>(clientRect.right - clientRect.left);
    const float currentClientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (currentClientWidth <= 0.0f || currentClientHeight <= 0.0f) {
        return;
    }

    const float screenScaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientWidth;
    const float screenScaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientHeight;
    const float scaledMouseX = static_cast<float>(mouse.posX) * screenScaleX;
    const float scaledMouseY = static_cast<float>(mouse.posY) * screenScaleY;

    float offsetX = 0.0f;
    float offsetY = 0.0f;

#if defined(USE_IMGUI) && !defined(NDEBUG)
    offsetX = static_cast<float>(WinApp::kWindowWidth - WinApp::kClientWidth) / 2.0f;
#endif

#ifdef NDEBUG
    // Releaseではクライアント領域を基準にマウス座標を合わせる。
    const float mouseX = static_cast<float>(mouse.posX) *
        (static_cast<float>(WinApp::kClientWidth) / currentClientWidth);
    const float mouseY = static_cast<float>(mouse.posY) *
        (static_cast<float>(WinApp::kClientHeight) / currentClientHeight);
#else
    // DebugではImGui表示分の横オフセットを差し引き、ゲーム画面内座標へ変換する。
    const float mouseX = scaledMouseX - offsetX;
    const float mouseY = scaledMouseY - offsetY;
#endif

    const float normalSize = 64.0f;
    const float glowSize = 78.0f;

    Vector2 leftGuidePosition = {
        leftGuideX + leftGuideOffset.x,
        centerY + leftGuideOffset.y + floatingOffsetY
    };
    Vector2 rightGuidePosition = {
        rightGuideX + rightGuideOffset.x,
        centerY + rightGuideOffset.y + floatingOffsetY
    };
    Vector2 upGuidePosition = {
        centerX + upGuideOffset.x,
        topGuideY + upGuideOffset.y + floatingOffsetY
    };
    Vector2 downGuidePosition = {
        centerX + downGuideOffset.x,
        bottomGuideY + downGuideOffset.y + floatingOffsetY
    };

    auto isMouseOverGuide = [&](Vector2 guidePosition) {
        return mouseX >= guidePosition.x &&
            mouseX <= guidePosition.x + normalSize &&
            mouseY >= guidePosition.y &&
            mouseY <= guidePosition.y + normalSize;
    };

    const bool hoverLeft = isMouseOverGuide(leftGuidePosition);
    const bool hoverRight = isMouseOverGuide(rightGuidePosition);
    const bool hoverUp = isMouseOverGuide(upGuidePosition);
    const bool hoverDown = isMouseOverGuide(downGuidePosition);

    cameraGuideLeftSprite_->SetPosition(leftGuidePosition);
    cameraGuideRightSprite_->SetPosition(rightGuidePosition);
    cameraGuideUpSprite_->SetPosition(upGuidePosition);
    cameraGuideDownSprite_->SetPosition(downGuidePosition);

    cameraGuideLeftSprite_->SetSize({ hoverLeft ? glowSize : normalSize, hoverLeft ? glowSize : normalSize });
    cameraGuideRightSprite_->SetSize({ hoverRight ? glowSize : normalSize, hoverRight ? glowSize : normalSize });
    cameraGuideUpSprite_->SetSize({ hoverUp ? glowSize : normalSize, hoverUp ? glowSize : normalSize });
    cameraGuideDownSprite_->SetSize({ hoverDown ? glowSize : normalSize, hoverDown ? glowSize : normalSize });

    cameraGuideLeftSprite_->SetRotation(0.0f);
    cameraGuideRightSprite_->SetRotation(0.0f);
    cameraGuideUpSprite_->SetRotation(0.0f);
    cameraGuideDownSprite_->SetRotation(0.0f);

    cameraGuideLeftSprite_->Update();
    cameraGuideRightSprite_->Update();
    cameraGuideUpSprite_->Update();
    cameraGuideDownSprite_->Update();
}

void GameplayUIManager::DrawSprites(bool isGamePlayMode, bool isFollowPlayerMode) {
    if (!isGamePlayMode) {
        return;
    }
    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    // 2D UIはSprite用パイプラインに切り替えてまとめて描画する。
    spriteCommon_->PreDraw();

    cameraGuideLeftSprite_->Draw();
    cameraGuideRightSprite_->Draw();
    cameraGuideUpSprite_->Draw();
    cameraGuideDownSprite_->Draw();

    if (isFollowPlayerMode) {
        if (cameraModePlayerSprite_) {
            cameraModePlayerSprite_->Draw();
        }
    } else {
        if (cameraModeStageSprite_) {
            cameraModeStageSprite_->Draw();
        }
    }
}

void GameplayUIManager::Draw3DPrompts(bool isGamePlayMode, Player* player, Object3dCommon* object3dCommon, ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle) {
    if (!isGamePlayMode || !player) {
        return;
    }

    const bool drawDoorPrompt = doorPromptObject_ && player->IsNearDoor();
    const bool drawLadderPrompt = ladderPromptObject_ && player->IsOnLadder();
    const bool drawPSwitchPrompt = pSwitchPromptObject_ && player->IsNearPSwitch();
    const bool drawKeyPrompt = keyPromptObject_ && player->IsNearKey();
    const bool drawKeyBlockPrompt = keyBlockPromptObject_ && player->IsNearKeyBlock();

    if (drawDoorPrompt ||
        drawLadderPrompt ||
        drawPSwitchPrompt ||
        drawKeyPrompt ||
        drawKeyBlockPrompt) {

        // 3D UIはプレイヤー強調表示用のライティングなしパスで描画し、見やすさを優先する。
        object3dCommon->PreDrawPlayerHighlight();

        if (drawDoorPrompt) {
            doorPromptObject_->Draw();
        }
        if (drawLadderPrompt) {
            ladderPromptObject_->Draw();
        }
        if (drawPSwitchPrompt) {
            pSwitchPromptObject_->Draw();
        }
        if (drawKeyPrompt) {
            keyPromptObject_->Draw();
        }
        if (drawKeyBlockPrompt) {
            keyBlockPromptObject_->Draw();
        }

        // 後続の通常3D描画に戻すため、パイプラインとシャドウSRVを復元する。
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowSrvHandle);
    }
}

void GameplayUIManager::Finalize() {
    // 所有しているUIリソースを明示的に破棄する。unique_ptrなので順序以外は自動解放される。
    doorPromptObject_.reset();
    doorPromptModel_.reset();
    pSwitchPromptObject_.reset();
    keyPromptObject_.reset();
    keyBlockPromptObject_.reset();
    ladderPromptObject_.reset();
    ladderPromptModel_.reset();
    cameraGuideLeftSprite_.reset();
    cameraGuideRightSprite_.reset();
    cameraGuideUpSprite_.reset();
    cameraGuideDownSprite_.reset();
    cameraModeStageSprite_.reset();
    cameraModePlayerSprite_.reset();
}
