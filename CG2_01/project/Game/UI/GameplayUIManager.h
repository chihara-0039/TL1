#pragma once
#include <memory>
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "Object3d.h"
#include "Model.h"
#include "Player.h"
#include "Camera.h"
#include "LightCamera.h"

// ゲームプレイ中に表示する2Dガイドと3D操作プロンプトをまとめて管理する。
class GameplayUIManager {
public:
    // UI表示に必要な描画共通リソースとモデルを初期化する。
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager, SpriteCommon* spriteCommon, Object3dCommon* object3dCommon);
    // プレイヤー位置やカメラ状態に応じて、表示すべきUIの位置と表示状態を更新する。
    void Update(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    // カメラ操作ガイドの表示時間と入力状態を更新する。
    void UpdateCameraGuide(bool isGamePlay, Input* input, WinApp* winApp);
    // 2DスプライトUIを描画する。
    void DrawSprites(bool isGamePlayMode, bool isFollowPlayerMode);
    // 扉、はしご、鍵などの3Dプロンプトを描画する。
    void Draw3DPrompts(bool isGamePlayMode, Player* player, Object3dCommon* object3dCommon, ID3D12GraphicsCommandList* commandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle);
    void Finalize();

private:
    void UpdateDoorPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdateLadderPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdatePSwitchPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdateKeyPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);
    void UpdateKeyBlockPrompt3D(bool isGamePlayMode, Player* player, Camera* camera, LightCamera* lightCamera);

    SpriteCommon* spriteCommon_ = nullptr;

    // カメラ回転ガイド用の方向スプライト。
    std::unique_ptr<Sprite> cameraGuideLeftSprite_;
    std::unique_ptr<Sprite> cameraGuideRightSprite_;
    std::unique_ptr<Sprite> cameraGuideUpSprite_;
    std::unique_ptr<Sprite> cameraGuideDownSprite_;
    uint32_t cameraGuideLeftTextureHandle_ = 0;
    uint32_t cameraGuideRightTextureHandle_ = 0;
    uint32_t cameraGuideUpTextureHandle_ = 0;
    uint32_t cameraGuideDownTextureHandle_ = 0;

    // カメラ追従/ステージ固定モード表示用スプライト。
    std::unique_ptr<Sprite> cameraModeStageSprite_;
    std::unique_ptr<Sprite> cameraModePlayerSprite_;
    uint32_t cameraModeStageTextureHandle_ = 0;
    uint32_t cameraModePlayerTextureHandle_ = 0;

    // 扉操作用の3D FキーUI。
    std::unique_ptr<Model> doorPromptModel_;
    std::unique_ptr<Object3d> doorPromptObject_;

    // はしご操作用の3D FキーUI。
    std::unique_ptr<Model> ladderPromptModel_;
    std::unique_ptr<Object3d> ladderPromptObject_;

    // Pスイッチ、鍵、鍵ブロック操作用の3Dプロンプト。
    std::unique_ptr<Object3d> pSwitchPromptObject_;
    std::unique_ptr<Object3d> keyPromptObject_;
    std::unique_ptr<Object3d> keyBlockPromptObject_;

    // カメラガイド表示の残り時間。
    float cameraGuideTime_ = 0.0f;
};
