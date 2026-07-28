#include "LightCamera.h"

void LightCamera::Initialize() {
    // デフォルト値で一度計算しておく
    Update({ 0.2f, -1.0f, 0.5f }, { 0.0f, 0.0f, 0.0f });
}

void LightCamera::Update(const Vector3& lightDirection, const Vector3& targetPos) {
    // 1. ライトの位置を決定
    // 平行光源に「位置」はありませんが、行列を作るためにターゲット（プレイヤー）から
    // 逆方向に十分離れた場所を便宜上の「ライトの位置」とします。
    float distance = 100.0f;
    Vector3 lightPos = {
        targetPos.x - lightDirection.x * distance,
        targetPos.y - lightDirection.y * distance,
        targetPos.z - lightDirection.z * distance
    };

    // 2. ビュー行列の作成
    // ライトの位置からターゲット（プレイヤー周辺）を真っ直ぐ見るように設定
    viewMatrix_ = Math::MakeLookAtMatrix(lightPos, targetPos, { 0.0f, 1.0f, 0.0f });

    // 3. 正投影行列の作成
    // 平行光源の影は、遠くのものが小さくならないように Orthographic を使うのが BotW 流！
    // 左右・上下の幅を shadowRange_ で指定します。
    projectionMatrix_ = Math::MakeOrthographicMatrix(
        -shadowRange_, shadowRange_,  // 左右の幅
        shadowRange_, -shadowRange_,  // 上下の幅
        0.1f, 300.0f                  // 前後のクリップ距離
    );

    // 4. 行列の合成 (View * Projection)
    viewProjectionMatrix_ = Math::Multiply(viewMatrix_, projectionMatrix_);
}