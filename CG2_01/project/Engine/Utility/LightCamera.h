#pragma once
#include "MyMath.h"

/// <summary>
/// 影を作るための「ライト視点カメラ」クラス。
/// 平行光源（太陽光）は遠近感がないため、正投影(Orthographic)を使用します。
/// </summary>
class LightCamera {
public:
    // 初期化
    void Initialize();

    // 更新：ライトの向きと、影を落としたい中心地点（プレイヤー等）を元に行列を計算
    void Update(const Vector3& lightDirection, const Vector3& targetPos);

    // ゲッター：ライト視点の「ビュープロジェクション行列」を取得
    const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }

private:
    Matrix4x4 viewMatrix_;           // ライト視点のビュー行列
    Matrix4x4 projectionMatrix_;     // ライト視点の正投影行列
    Matrix4x4 viewProjectionMatrix_; // 合成済み行列

    // 影を記録する空間の広さ（値を小さくするとカメラ周辺の影がより高解像度になる）
    // ステージ全体をカバーできるように 64.0f に設定
    float shadowRange_ = 64.0f;
};