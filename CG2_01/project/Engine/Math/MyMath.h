#pragma once
#include <cmath>

//=======================
// 型定義
//=======================
struct Vector2 {
    float x, y;
};
struct Vector3 {
    float x, y, z;
};
struct Vector4 {
    float x, y, z, w;
};
struct Matrix3x3 {
    float m[3][3];
};
struct Matrix4x4 {
    float m[4][4];
};

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

struct Quaternion {
    float x, y, z, w;
};

//=======================
// 数学関数群
//=======================
namespace Math {

    Matrix4x4 MakeIdentity4x4();

    // 拡大縮小行列
    Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& s);

    // 回転行列
    Matrix4x4 MakeRotateXMatrix(float radian);
    Matrix4x4 MakeRotateYMatrix(float radian);
    Matrix4x4 MakeRotateZMatrix(float radian);

    // 平行移動行列
    Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

    // 行列の積
    Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

    // アフィン行列
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

    // 逆行列 (引数を const Matrix4x4& に統一)
    Matrix4x4 Inverse(const Matrix4x4& m);
    Matrix4x4 Transpose(const Matrix4x4& m);

    // 透視投影行列
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

    // 正射影行列 (Spriteで必要)
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

    // ビューポート行列 (Spriteで必要)
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

    // 正規化
    Vector3 Normalize(const Vector3& v);

    // ベクトルの減算 (v1 - v2)
    Vector3 Subtract(const Vector3& v1, const Vector3& v2);
    // ベクトルの外積
    Vector3 Cross(const Vector3& v1, const Vector3& v2);
    // ビュー行列 (LookAt) の作成
    Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);

    // クォータニオン関連
    Quaternion Multiply(const Quaternion& q1, const Quaternion& q2);
    Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t);
    Matrix4x4 MakeRotateMatrix(const Quaternion& q);
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);
    Quaternion MakeQuaternionFromEuler(const Vector3& euler);
    Vector3 ToEuler(const Quaternion& q);
}
