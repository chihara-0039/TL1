#include "TitleScene.h"

void TitleScene::Initialize(Object3dCommon* objCommon, Input* input) {
    object3dCommon_ = objCommon;
    input_ = input;

    // カメラ用変数
    cameraPos_ = { 0.0f, 2.0f, -20.0f };
    cameraRot_ = { 0.0f, 0.0f, 0.0f };

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);

    // モデル読み込み（好きなモデルに変更OK）
    titleModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources/Models/title",
        "title.obj",
        object3dCommon_->GetTextureManager()
    ));

    // オブジェクト生成
    titleObject_ = std::make_unique<Object3d>();
    titleObject_->Initialize(object3dCommon_);
    titleObject_->SetModel(titleModel_.get());

    position_ = { 0, -10, 10 };
    rotation_ = { 0, 0, 0 };

    titleObject_->SetPosition(position_);
    titleObject_->SetRotation(rotation_);

    // 雲モデルの読み込み
    cloudModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources/Models/soapBubbles",
        "soapBubbles.obj",
        object3dCommon_->GetTextureManager()
    ));

    // Space UIモデル読み込み
    pressSpaceModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources/UI/pressSpace",
        "pressSpace.obj",
        object3dCommon_->GetTextureManager()
    ));

    pressSpaceObject_ = std::make_unique<Object3d>();
    pressSpaceObject_->Initialize(object3dCommon_);
    pressSpaceObject_->SetModel(pressSpaceModel_.get());

    pressSpacePos_ = { 0.0f, -4.0f, 10.0f };
    pressSpaceRot_ = { 0.0f, 0.0f, 0.0f };
    pressSpaceScale_ = { 1.5f, 1.5f, 1.5f };

    pressSpaceObject_->SetPosition(pressSpacePos_);
    pressSpaceObject_->SetRotation(pressSpaceRot_);
    pressSpaceObject_->SetScale(pressSpaceScale_);
    pressSpaceObject_->SetEnableLighting(false);


    // 雲の生成 (StageRendererと同様)
    int cloudCount = 12;
    for (int i = 0; i < cloudCount; ++i) {
        CloudInstance cloud;
        // タイトル画面用のランダム座標
        float rx = ((float)rand() / RAND_MAX) * 80.0f - 40.0f;
        float ry = ((float)rand() / RAND_MAX) * 10.0f + 5.0f; // 少し高め
        float rz = ((float)rand() / RAND_MAX) * 40.0f - 10.0f;
        cloud.basePosition = { rx, ry, rz };

        float speedX = ((float)rand() / RAND_MAX) * 0.4f + 0.1f;
        cloud.speed = { speedX, 0.0f, 0.0f };

        cloud.floatTimer = ((float)rand() / RAND_MAX) * 6.28f;
        cloud.floatSpeed = ((float)rand() / RAND_MAX) * 0.3f + 0.1f;

        int partCount = (rand() % 3) + 3;
        for (int j = 0; j < partCount; ++j) {
            float ox = ((float)rand() / RAND_MAX) * 4.0f - 2.0f;
            float oy = ((float)rand() / RAND_MAX) * 1.5f - 0.75f;
            float oz = ((float)rand() / RAND_MAX) * 4.0f - 2.0f;
            cloud.localOffsets.push_back({ ox, oy, oz });

            float s = ((float)rand() / RAND_MAX) * 2.0f + 1.5f;
            cloud.localScales.push_back({ s, s * 0.5f, s });
        }

        for (int j = 0; j < partCount; ++j) {
            std::unique_ptr<Object3d> obj = std::make_unique<Object3d>();
            obj->Initialize(object3dCommon_);
            obj->SetModel(cloudModel_.get());
            obj->SetEnableLighting(true);
            obj->SetShininess(0.0f);
            obj->SetMetallic(0.0f);

            Vector3 partPos = {
                cloud.basePosition.x + cloud.localOffsets[j].x,
                cloud.basePosition.y + cloud.localOffsets[j].y,
                cloud.basePosition.z + cloud.localOffsets[j].z
            };
            obj->SetPosition(partPos);
            obj->SetScale(cloud.localScales[j]);
            obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            cloud.objects.push_back(std::move(obj));
        }
        clouds_.push_back(std::move(cloud));
    }
}

void TitleScene::Update() {

    timer_ += 0.02f;

    // 渦巻き
    if (spiralAngle_ < 6.28f) {
        spiralAngle_ += 0.05f;

        // 下から上がる
        if (position_.y < 0) {
            position_.y += 0.1f;
        }

        float radius = 2.0f * (1.0f - (position_.y + 10.0f) / 10.0f);
        if (radius < 0) radius = 0;

        position_.x = std::cos(spiralAngle_) * radius;
        position_.z = std::sin(spiralAngle_) * radius;

        rotation_.y += 0.02f;
    }

    // 常時回転
    rotation_.y += 0.02f;

    // ★ Objectに反映
    titleObject_->SetPosition(position_);
    titleObject_->SetRotation(rotation_);


    // =========================
   // カメラ演出
   // =========================

   // ズームイン
    if (cameraPos_.z < -10.0f) {
        cameraPos_.z += 0.03f;
    }

    // 少し見下ろす
    cameraRot_.x = 0.25f;

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);
    camera_.Update();


    // =========================
    // カメラ反映
    // =========================
    const Matrix4x4& view = camera_.GetViewMatrix();
    const Matrix4x4& proj = camera_.GetProjectionMatrix();

    titleObject_->SetCamera(view, proj);
    titleObject_->Update(Math::MakeIdentity4x4());


    // =========================
    // Space UI更新
    // タイトルが上がりきったら表示して、その場で止める
    // =========================
    if (position_.y >= 0.0f) {
        isPressSpaceVisible_ = true;
    }

    if (pressSpaceObject_ && isPressSpaceVisible_) {
        pressSpaceTimer_ += 0.05f;

        float scale = 1.0f + std::sin(pressSpaceTimer_) * 0.05f;

        pressSpaceObject_->SetPosition(pressSpacePos_);
        pressSpaceObject_->SetRotation(pressSpaceRot_);
        pressSpaceObject_->SetScale({
            pressSpaceScale_.x * scale,
            pressSpaceScale_.y * scale,
            pressSpaceScale_.z * scale
            });

        pressSpaceObject_->SetCamera(view, proj);
        pressSpaceObject_->Update(Math::MakeIdentity4x4());
    }

    // =========================
    // 雲の更新
    // =========================
    float dt = 1.0f / 60.0f; // 簡易的なdt
    for (auto& cloud : clouds_) {
        cloud.basePosition.x += cloud.speed.x * dt;
        if (cloud.basePosition.x > 40.0f) {
            cloud.basePosition.x = -40.0f;
        }
        cloud.floatTimer += cloud.floatSpeed * dt;
        float offsetY = std::sin(cloud.floatTimer) * 0.4f;

        for (size_t i = 0; i < cloud.objects.size(); ++i) {
            Vector3 partPos = {
                cloud.basePosition.x + cloud.localOffsets[i].x,
                cloud.basePosition.y + cloud.localOffsets[i].y + offsetY,
                cloud.basePosition.z + cloud.localOffsets[i].z
            };
            cloud.objects[i]->SetPosition(partPos);
            cloud.objects[i]->SetCamera(view, proj);
            cloud.objects[i]->Update(Math::MakeIdentity4x4());
        }
    }

    // =========================
    // シーン遷移
    // =========================
    if (input_->TriggerKey(DIK_SPACE)) {
        isFinished_ = true;
    }
}

void TitleScene::Draw() {
    titleObject_->Draw();

    

    for (const auto& cloud : clouds_) {
        for (const auto& obj : cloud.objects) {
            obj->Draw();
        }
    }

    if (pressSpaceObject_ && isPressSpaceVisible_) {
        pressSpaceObject_->Draw();
    }
}
