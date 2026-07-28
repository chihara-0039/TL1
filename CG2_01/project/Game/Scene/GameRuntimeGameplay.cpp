// プレイヤー操作、ステージルール、画面遷移、BGM更新を担当する。
#include "GameRuntime.h"
#include "Goal.h"
#include <algorithm>
#include <cmath>
#include "externals/imgui/imgui.h"

void GameRuntime::UpdateGamePlay() {
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.UpdateGameplay(*input, postProcess_);
    postEffectShowcaseController_.DrawGameplayImGui(postProcess_);

    if (input->TriggerKey(DIK_C)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            fpsCameraYaw_ = player_->GetRotation().y;
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        if (input->TriggerKey(DIK_V)) {
            bool wasFollowingPlayer = gameplayCameraController_.IsFollowPlayerMode();
            gameplayCameraController_.SetFollowPlayerMode(!wasFollowingPlayer);
            if (!wasFollowingPlayer && player_) {
                Vector3 playerPivotPosition = player_->GetPosition();
                playerPivotPosition.y += 0.8f;
                gameplayCameraController_.SetCameraPivot(playerPivotPosition);
            } else if (wasFollowingPlayer && stageSelect_) {
                gameplayCameraController_.ResetCamera(
                    camera.get(),
                    player_.get(),
                    stageMap_,
                    stageSelect_->GetSelectedIndex());
            }
        }
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
        camera->Update();
    } else {
        camera->SetFov(0.9f);

        bool isGuiCaptured = false;
#ifndef NDEBUG

        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif
        const auto& mouse = input->GetMouseState();

        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(winApp->GetHwnd(), &rect);
            float clientWidth = static_cast<float>(rect.right - rect.left);
            float clientHeight = static_cast<float>(rect.bottom - rect.top);
            if (clientWidth > 0.0f && clientHeight > 0.0f) {
                float screenScaleX = static_cast<float>(WinApp::kWindowWidth) / clientWidth;
                float screenScaleY = static_cast<float>(WinApp::kWindowHeight) / clientHeight;
                float scaledMouseX = static_cast<float>(mouse.posX) * screenScaleX;
                float scaledMouseY = static_cast<float>(mouse.posY) * screenScaleY;

                const float edgeRatio = 0.15f;
                const float rotateSpeed = 0.03f;
                float leftEdge = WinApp::kWindowWidth * edgeRatio;
                float rightEdge = WinApp::kWindowWidth * (1.0f - edgeRatio);
                float topEdge = WinApp::kWindowHeight * edgeRatio;
                float bottomEdge = WinApp::kWindowHeight * (1.0f - edgeRatio);

                if (scaledMouseX < leftEdge) {
                    fpsCameraYaw_ -= rotateSpeed;
                } else if (scaledMouseX > rightEdge) {
                    fpsCameraYaw_ += rotateSpeed;
                }

                if (scaledMouseY < topEdge) {
                    fpsCameraPitch_ -= rotateSpeed;
                } else if (scaledMouseY > bottomEdge) {
                    fpsCameraPitch_ += rotateSpeed;
                }
            }
        }

        const float keyRotateSpeed = 0.03f;
        if (input->PushKey(DIK_LEFT)) { fpsCameraYaw_ -= keyRotateSpeed; }
        if (input->PushKey(DIK_RIGHT)) { fpsCameraYaw_ += keyRotateSpeed; }
        if (input->PushKey(DIK_UP)) { fpsCameraPitch_ -= keyRotateSpeed; }
        if (input->PushKey(DIK_DOWN)) { fpsCameraPitch_ += keyRotateSpeed; }
        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        if (player_) {
            Vector3 playerPosition = player_->GetPosition();
            camera->SetPosition({ playerPosition.x, playerPosition.y + 1.2f, playerPosition.z });
            camera->SetRotation({ fpsCameraPitch_, fpsCameraYaw_, 0.0f });
        }
        camera->Update();
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(currentMode_ == AppMode::GamePlay, input.get(), winApp.get());
    }

    bool isInventoryOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();

    if (stageSelect_ && stageSelect_->GetSelectedFileName() == "tutorial.txt" && tutorialSprite_ && !isInventoryOpen) {
        tutorialSprite_->Update();
    }

    if ((currentMode_ == AppMode::GamePlay_BlockPlace || isInventoryOpen) && placementTutorialSprite_) {
        placementTutorialSprite_->Update();
    }

    float fixedDeltaTime = 1.0f / 60.0f;
    totalTime_ += fixedDeltaTime;
    stageMap_.Update(fixedDeltaTime, player_ ? player_->GetPosition() : Vector3{ 0, 0, 0 });
    stageRenderer_->UpdateEffect(stageMap_);

    if (player_) {
        float cameraYawForMovement = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, cameraYawForMovement, lightVP, dxCommon.get());
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(
        stageMap_,
        backupMap_,
        stageRenderer_.get(),
        player_.get(),
        &blockInventory_,
        &bubblePickupController_,
        &blockPlacementController_,
        &stageEditorController_);

    Vector3 pPos = player_ ? player_->GetPosition() : Vector3{};
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, stageMap_)) {
        isGoalReached_ = true;
    }

    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen();
        }
    }

    if (isGoalReached_) {
        if (clearGuideSprite_) {
            clearGuideSprite_->Update();
        }

        // ゴール後は明示操作でステージ選択へ戻す。
        // 押した瞬間に復元して、次に同じステージを始めても崩壊床等が残らないようにする。
        if (input->TriggerKey(DIK_SPACE)) {
            stageMap_ = backupMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);
            bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            stageSelect_->Initialize(object3dCommon.get(), input.get());
            isGoalReached_ = false;
            RequestSceneChange(SceneType::StageSelect);
        }
    }
}

void GameRuntime::UpdateGamePlayBlockPlace() {
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.UpdateGameplay(*input, postProcess_);
    postEffectShowcaseController_.DrawGameplayImGui(postProcess_);

    const Int3& cursor = mapCursor_->GetIndex();
    if (input->TriggerKey(DIK_R)) {
        placeRotationY_ += 1.5707963f;
        if (placeRotationY_ >= 6.0f) {
            placeRotationY_ = 0.0f;
        }
    }

    stageEditorController_.HandleCursorInput(
        input.get(),
        stageMap_,
        mapCursor_.get(),
        lightCamera_.get(),
        camera.get());

    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_);
    }

    static bool prevMouse0 = false;
    bool mouseJustPressed = input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0 = input->GetMouseState().buttons[0];
    bool mouseTrigger = false;
    if (mouseJustPressed && (!blockInventoryUI_ || !blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        if (blockPlacementController_.TryPlace(cursor, placeRotationY_)) {
            bool hasRest = (selectedType == BlockType::Ground)
                || blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                RequestSceneChange(SceneType::GamePlay);
                placeRotationY_ = 0.0f;
                if (stageRenderer_) {
                    stageRenderer_->ClearPlacementPreview();
                }
            }
        }
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B)) {
        RequestSceneChange(SceneType::GamePlay);
        placeRotationY_ = 0.0f;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview();
        }
    }

    stageEditorController_.HandleCameraInput(input.get(), camera.get());
}

void GameRuntime::UpdateStageSelect() {
    stageSelect_->Update();
    if (stageSelect_->IsFnished()) {
        std::string path = "Resources/Stages/" + stageSelect_->GetSelectedFileName();
        if (std::filesystem::exists(path)) {
            // グリッドステージ開始時はBlenderステージの衝突・Spawnを無効化する。
            blenderStageActive_ = false;
            if (player_) {
                player_->SetExternalCollisionBoxes(nullptr);
            }
            stageMap_.LoadFromFile(path);
            backupMap_ = stageMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);

            playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());

            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            gameplayCameraController_.ResetCamera(
                camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            blockInventory_.Initialize(0);
        }
        RequestSceneChange(SceneType::GamePlay);
    }
}


void GameRuntime::UpdateSceneTransition() {
    if ((currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)
        && input->TriggerKey(DIK_ESCAPE)) {
        stageMap_ = backupMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);
        
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        isGoalReached_ = false;
        if (player_) { player_->Respawn(); }
        RequestSceneChange(SceneType::StageSelect);
    }
}

