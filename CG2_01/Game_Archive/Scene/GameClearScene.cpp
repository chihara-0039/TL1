#include "GameClearScene.h"
#include <memory>


void GameClearScene::Initialize(Object3dCommon* objCommon) {
    object3dCommon_ = objCommon;

    //初期化　5/11 小林
    timer_ = 0.0f;
    isAllFinished_ = false;
    finishTimer_ = 0.0f;

    cameraPos_ = { 0, 2, -25 };
    cameraRot_ = { 0.25f, 0, 0 };

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);

    letters_.clear();

    for (int i = 0; i < text_.size(); i++) {

        Letter letter;

        std::string name(1, text_[i]);

        letter.model = std::unique_ptr<Model>(Model::CreateFromOBJ(
            object3dCommon_->GetDxCommon(),
            "Resources/Models/ClearText/" + name,
            name + ".obj",
            object3dCommon_->GetTextureManager()
        ));

        letter.object = std::make_unique<Object3d>();
        letter.object->Initialize(object3dCommon_);
        letter.object->SetModel(letter.model.get());

        letter.object->SetRotation({ 0.0f,3.141592f,0.0f });

        letter.position = { -6.0f + i * 1.2f, -10.0f, 0.0f };
        letter.baseY = letter.position.y;

        letters_.push_back(std::move(letter));
    }
}

void GameClearScene::Update() {

    timer_ += 1.0f;

    bool allFinished = true;

    for (int i = 0; i < letters_.size(); i++) {

        if (timer_ > i * 20) {

            if (!letters_[i].isVisible) {
                letters_[i].isVisible = true;
                letters_[i].bounceTime = 0.0f;
            }

            // 上昇
            if (letters_[i].baseY < -3.0f) {
                letters_[i].baseY += 0.4f;
            }

            // バウンド
            if (letters_[i].bounceTime < 30.0f) {

                letters_[i].bounceTime += 1.0f;

                float offset = std::sin(letters_[i].bounceTime * 0.3f) * 0.8f;

                letters_[i].position.y = letters_[i].baseY + offset;

                allFinished = false;
            } else {
                letters_[i].position.y = letters_[i].baseY;
            }
        } else {
            allFinished = false;
        }

        // Objectに反映
        letters_[i].object->SetPosition(letters_[i].position);
        letters_[i].object->SetScale(letters_[i].scale);

        // カメラ設定
        const Matrix4x4& view = camera_.GetViewMatrix();
        const Matrix4x4& proj = camera_.GetProjectionMatrix();

        letters_[i].object->SetCamera(view, proj);
        letters_[i].object->Update(Math::MakeIdentity4x4());
    }

    // 全部終わった後のドン
    if (allFinished) {
        isAllFinished_ = true;
    }

    if (isAllFinished_) {
        finishTimer_ += 1.0f;

        float scale = 1.0f;

        if (finishTimer_ < 10.0f) {
            scale = 1.0f + finishTimer_ * 0.05f;
        } else if (finishTimer_ < 20.0f) {
            scale = 1.5f - (finishTimer_ - 10.0f) * 0.05f;
        }

        for (auto& letter : letters_) {
            letter.scale = { scale, scale, scale };
        }
    }

    // カメラズーム
    // カメラズーム（文字が揃うまで）
    if (!isAllFinished_) {
        if (cameraPos_.z < -10.0f) {
            cameraPos_.z += 0.02f;
        }
    }

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);
    camera_.Update();
}

void GameClearScene::Draw() {
    for (auto& letter : letters_) {
        if (letter.isVisible) {
            letter.object->Draw();
        }
    }
}

void GameClearScene::SkipAnimation() {

    timer_ = 9999.0f;

    for (int i = 0; i < letters_.size(); i++) {

        letters_[i].isVisible = true;

        // 通常演出の最終位置と同じにする
        letters_[i].baseY = -3.0f;
        letters_[i].position = {
            -6.0f + i * 1.2f,
            -3.0f,
            0.0f
        };

        // バウンド終了状態
        letters_[i].bounceTime = 30.0f;

        // 通常サイズ
        letters_[i].scale = { 1.0f, 1.0f, 1.0f };

        // Objectにも即反映
        letters_[i].object->SetPosition(letters_[i].position);
        letters_[i].object->SetScale(letters_[i].scale);
    }

    isAllFinished_ = true;
    finishTimer_ = 20.0f;

    cameraPos_.z = -20.0f;
    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);
    camera_.Update();

    const Matrix4x4& view = camera_.GetViewMatrix();
    const Matrix4x4& proj = camera_.GetProjectionMatrix();

    for (auto& letter : letters_) {
        letter.object->SetCamera(view, proj);
        letter.object->Update(Math::MakeIdentity4x4());
    }

}