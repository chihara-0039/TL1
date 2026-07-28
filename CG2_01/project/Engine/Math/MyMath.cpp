#include "MyMath.h"
#include <cassert>
#include <cmath>

namespace Math {

    // 単位行列を作成する。何も変換しない初期値として使う。
    Matrix4x4 MakeIdentity4x4() {
        Matrix4x4 result{};
        for (int index = 0; index < 4; ++index) {
            result.m[index][index] = 1.0f;
        }
        return result;
    }

    // 拡大縮小行列を作成する。
    Matrix4x4 Matrix4x4MakeScaleMatrix(const Vector3& scale) {
        Matrix4x4 result{};
        result.m[0][0] = scale.x;
        result.m[1][1] = scale.y;
        result.m[2][2] = scale.z;
        result.m[3][3] = 1.0f;
        return result;
    }

    // X軸回転行列を作成する。
    Matrix4x4 MakeRotateXMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = std::cos(radian);
        result.m[1][2] = std::sin(radian);
        result.m[2][1] = -std::sin(radian);
        result.m[2][2] = std::cos(radian);
        result.m[3][3] = 1.0f;
        return result;
    }

    // Y軸回転行列を作成する。
    Matrix4x4 MakeRotateYMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = std::cos(radian);
        result.m[0][2] = -std::sin(radian);
        result.m[1][1] = 1.0f;
        result.m[2][0] = std::sin(radian);
        result.m[2][2] = std::cos(radian);
        result.m[3][3] = 1.0f;
        return result;
    }

    // Z軸回転行列を作成する。
    Matrix4x4 MakeRotateZMatrix(float radian) {
        Matrix4x4 result{};
        result.m[0][0] = std::cos(radian);
        result.m[0][1] = std::sin(radian);
        result.m[1][0] = -std::sin(radian);
        result.m[1][1] = std::cos(radian);
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        return result;
    }

    // 平行移動行列を作成する。
    Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
        Matrix4x4 result{};
        result.m[0][0] = 1.0f;
        result.m[1][1] = 1.0f;
        result.m[2][2] = 1.0f;
        result.m[3][3] = 1.0f;
        result.m[3][0] = translate.x;
        result.m[3][1] = translate.y;
        result.m[3][2] = translate.z;
        return result;
    }

    // 4x4行列同士を乗算する。
    Matrix4x4 Multiply(const Matrix4x4& matrixA, const Matrix4x4& matrixB) {
        Matrix4x4 result{};
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                for (int element = 0; element < 4; ++element) {
                    result.m[row][column] += matrixA.m[row][element] * matrixB.m[element][column];
                }
            }
        }
        return result;
    }

    // スケール、回転、平行移動を合成したアフィン行列を作成する。
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
        Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
        Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
        Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
        Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);
        Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

        Matrix4x4 rotateMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));
        return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    }

    // 4x4行列の逆行列を計算する。逆行列が存在しない場合は単位行列を返す。
    Matrix4x4 Inverse(const Matrix4x4& matrix) {
        float a = matrix.m[0][0], b = matrix.m[0][1], c = matrix.m[0][2], d = matrix.m[0][3];
        float e = matrix.m[1][0], f = matrix.m[1][1], g = matrix.m[1][2], h = matrix.m[1][3];
        float i = matrix.m[2][0], j = matrix.m[2][1], k = matrix.m[2][2], l = matrix.m[2][3];
        float n = matrix.m[3][0], o = matrix.m[3][1], p = matrix.m[3][2], q = matrix.m[3][3];

        float determinant = a * f * k * q + a * g * l * o + a * h * j * p
            + b * e * l * p + b * g * i * q + b * h * k * n
            + c * e * j * q + c * f * l * n + c * h * i * o
            + d * e * k * o + d * f * i * p + d * g * j * n
            - a * f * l * p - a * g * j * q - a * h * k * o
            - b * e * k * q - b * g * l * n - b * h * i * p
            - c * e * l * o - c * f * i * q - c * h * j * n
            - d * e * j * p - d * f * k * n - d * g * i * o;

        if (determinant == 0.0f) {
            return MakeIdentity4x4();
        }

        float inverseDeterminant = 1.0f / determinant;
        Matrix4x4 result{};

        result.m[0][0] = (f * k * q + g * l * o + h * j * p - f * l * p - g * j * q - h * k * o) * inverseDeterminant;
        result.m[0][1] = (-b * k * q - c * l * o - d * j * p + b * l * p + c * j * q + d * k * o) * inverseDeterminant;
        result.m[0][2] = (b * g * q + c * h * o + d * f * p - b * h * p - c * f * q - d * g * o) * inverseDeterminant;
        result.m[0][3] = (-b * g * l - c * h * j - d * f * k + b * h * k + c * f * l + d * g * j) * inverseDeterminant;

        result.m[1][0] = (-e * k * q - g * l * n - h * i * p + e * l * p + g * i * q + h * k * n) * inverseDeterminant;
        result.m[1][1] = (a * k * q + c * l * n + d * i * p - a * l * p - c * i * q - d * k * n) * inverseDeterminant;
        result.m[1][2] = (-a * g * q - c * h * n - d * e * p + a * h * p + c * e * q + d * g * n) * inverseDeterminant;
        result.m[1][3] = (a * g * l + c * h * i + d * e * k - a * h * k - c * e * l - d * g * i) * inverseDeterminant;

        result.m[2][0] = (e * j * q + f * l * n + h * i * o - e * l * o - f * i * q - h * j * n) * inverseDeterminant;
        result.m[2][1] = (-a * j * q - b * l * n - d * i * o + a * l * o + b * i * q + d * j * n) * inverseDeterminant;
        result.m[2][2] = (a * f * q + b * h * n + d * e * o - a * h * o - b * e * q - d * f * n) * inverseDeterminant;
        result.m[2][3] = (-a * f * l - b * h * i - d * e * j + a * h * j + b * e * l + d * f * i) * inverseDeterminant;

        result.m[3][0] = (-e * j * p - f * k * n - g * i * o + e * k * o + f * i * p + g * j * n) * inverseDeterminant;
        result.m[3][1] = (a * j * p + b * k * n + c * i * o - a * k * o - b * i * p - c * j * n) * inverseDeterminant;
        result.m[3][2] = (-a * f * p - b * g * n - c * e * o + a * g * o + b * e * p + c * f * n) * inverseDeterminant;
        result.m[3][3] = (a * f * k + b * g * i + c * e * j - a * g * j - b * e * k - c * f * i) * inverseDeterminant;

        return result;
    }

    Matrix4x4 Transpose(const Matrix4x4& matrix) {
        Matrix4x4 result{};
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                result.m[row][column] = matrix.m[column][row];
            }
        }
        return result;
    }

    // 透視投影行列を作成する。3D空間を遠近感つきでスクリーンへ投影する。
    Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
        Matrix4x4 result{};
        float heightScale = 1.0f / std::tan(fovY / 2.0f);
        float widthScale = heightScale / aspectRatio;

        result.m[0][0] = widthScale;
        result.m[1][1] = heightScale;
        result.m[2][2] = farClip / (farClip - nearClip);
        result.m[2][3] = 1.0f;
        result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
        return result;
    }

    // 正射影行列を作成する。2D描画やUIのように遠近感を付けない投影で使う。
    Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
        Matrix4x4 result = {};
        result.m[0][0] = 2.0f / (right - left);
        result.m[1][1] = 2.0f / (top - bottom);
        result.m[2][2] = 1.0f / (farClip - nearClip);
        result.m[3][3] = 1.0f;
        result.m[3][0] = (left + right) / (left - right);
        result.m[3][1] = (top + bottom) / (bottom - top);
        result.m[3][2] = nearClip / (nearClip - farClip);
        return result;
    }

    // ビューポート行列を作成する。NDC座標を画面ピクセル座標へ変換する。
    Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
        Matrix4x4 result{};
        result.m[0][0] = width / 2.0f;
        result.m[1][1] = -height / 2.0f;
        result.m[2][2] = maxDepth - minDepth;
        result.m[3][0] = left + width / 2.0f;
        result.m[3][1] = top + height / 2.0f;
        result.m[3][2] = minDepth;
        result.m[3][3] = 1.0f;
        return result;
    }

    // ベクトルの差を求める。
    Vector3 Subtract(const Vector3& vectorA, const Vector3& vectorB) {
        return { vectorA.x - vectorB.x, vectorA.y - vectorB.y, vectorA.z - vectorB.z };
    }

    // 2つのベクトルに垂直なベクトルを求める外積。
    Vector3 Cross(const Vector3& vectorA, const Vector3& vectorB) {
        return {
            vectorA.y * vectorB.z - vectorA.z * vectorB.y,
            vectorA.z * vectorB.x - vectorA.x * vectorB.z,
            vectorA.x * vectorB.y - vectorA.y * vectorB.x
        };
    }

    // LookAt のビュー行列を作成する。eye から target を見るカメラ座標系を作る。
    Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up) {
        // Z軸: 視線方向。
        Vector3 zAxis = Normalize(Subtract(target, eye));
        // X軸: 上方向と視線方向の外積から右方向を作る。
        Vector3 xAxis = Normalize(Cross(up, zAxis));
        // Y軸: 視線方向と右方向から再計算し、直交した上方向にする。
        Vector3 yAxis = Cross(zAxis, xAxis);

        Matrix4x4 result = MakeIdentity4x4();
        result.m[0][0] = xAxis.x; result.m[0][1] = yAxis.x; result.m[0][2] = zAxis.x;
        result.m[1][0] = xAxis.y; result.m[1][1] = yAxis.y; result.m[1][2] = zAxis.y;
        result.m[2][0] = xAxis.z; result.m[2][1] = yAxis.z; result.m[2][2] = zAxis.z;

        // カメラ位置を各軸へ射影し、ビュー空間への移動成分にする。
        result.m[3][0] = -(xAxis.x * eye.x + xAxis.y * eye.y + xAxis.z * eye.z);
        result.m[3][1] = -(yAxis.x * eye.x + yAxis.y * eye.y + yAxis.z * eye.z);
        result.m[3][2] = -(zAxis.x * eye.x + zAxis.y * eye.y + zAxis.z * eye.z);

        return result;
    }

    // ベクトルを正規化する。長さ0の場合は元の値を返す。
    Vector3 Normalize(const Vector3& vector) {
        float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        if (length != 0.0f) {
            return { vector.x / length, vector.y / length, vector.z / length };
        }
        return vector;
    }

    // クォータニオン同士を合成する。
    Quaternion Multiply(const Quaternion& quaternionA, const Quaternion& quaternionB) {
        Quaternion result;
        result.x = quaternionA.w * quaternionB.x + quaternionA.x * quaternionB.w + quaternionA.y * quaternionB.z - quaternionA.z * quaternionB.y;
        result.y = quaternionA.w * quaternionB.y - quaternionA.x * quaternionB.z + quaternionA.y * quaternionB.w + quaternionA.z * quaternionB.x;
        result.z = quaternionA.w * quaternionB.z + quaternionA.x * quaternionB.y - quaternionA.y * quaternionB.x + quaternionA.z * quaternionB.w;
        result.w = quaternionA.w * quaternionB.w - quaternionA.x * quaternionB.x - quaternionA.y * quaternionB.y - quaternionA.z * quaternionB.z;
        return result;
    }

    // 2つの回転を球面線形補間する。アニメーションの滑らかな回転補間に使う。
    Quaternion Slerp(const Quaternion& quaternionA, const Quaternion& quaternionB, float rate) {
        float cosHalfTheta = quaternionA.x * quaternionB.x + quaternionA.y * quaternionB.y + quaternionA.z * quaternionB.z + quaternionA.w * quaternionB.w;

        Quaternion targetQuaternion = quaternionB;
        if (cosHalfTheta < 0.0f) {
            targetQuaternion.x = -quaternionB.x;
            targetQuaternion.y = -quaternionB.y;
            targetQuaternion.z = -quaternionB.z;
            targetQuaternion.w = -quaternionB.w;
            cosHalfTheta = -cosHalfTheta;
        }

        if (cosHalfTheta >= 1.0f - 1e-5f) {
            Quaternion result;
            result.x = quaternionA.x + rate * (targetQuaternion.x - quaternionA.x);
            result.y = quaternionA.y + rate * (targetQuaternion.y - quaternionA.y);
            result.z = quaternionA.z + rate * (targetQuaternion.z - quaternionA.z);
            result.w = quaternionA.w + rate * (targetQuaternion.w - quaternionA.w);

            float length = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
            if (length > 0.0f) {
                result.x /= length;
                result.y /= length;
                result.z /= length;
                result.w /= length;
            }
            return result;
        }

        float halfTheta = std::acos(cosHalfTheta);
        float sinHalfTheta = std::sin(halfTheta);

        float ratioA = std::sin((1.0f - rate) * halfTheta) / sinHalfTheta;
        float ratioB = std::sin(rate * halfTheta) / sinHalfTheta;

        Quaternion result;
        result.x = quaternionA.x * ratioA + targetQuaternion.x * ratioB;
        result.y = quaternionA.y * ratioA + targetQuaternion.y * ratioB;
        result.z = quaternionA.z * ratioA + targetQuaternion.z * ratioB;
        result.w = quaternionA.w * ratioA + targetQuaternion.w * ratioB;
        return result;
    }

    // クォータニオンから回転行列を作成する。
    Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion) {
        Matrix4x4 result = MakeIdentity4x4();
        float xx = quaternion.x * quaternion.x;
        float yy = quaternion.y * quaternion.y;
        float zz = quaternion.z * quaternion.z;
        float xy = quaternion.x * quaternion.y;
        float xz = quaternion.x * quaternion.z;
        float yz = quaternion.y * quaternion.z;
        float wx = quaternion.w * quaternion.x;
        float wy = quaternion.w * quaternion.y;
        float wz = quaternion.w * quaternion.z;

        result.m[0][0] = 1.0f - 2.0f * (yy + zz);
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);

        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - 2.0f * (xx + zz);
        result.m[1][2] = 2.0f * (yz + wx);

        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - 2.0f * (xx + yy);

        return result;
    }

    // クォータニオン回転を使ったアフィン行列を作成する。
    Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate) {
        Matrix4x4 scaleMatrix = Matrix4x4MakeScaleMatrix(scale);
        Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
        Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

        return Multiply(Multiply(scaleMatrix, rotateMatrix), translateMatrix);
    }

    // オイラー角からクォータニオンを作成する。
    Quaternion MakeQuaternionFromEuler(const Vector3& euler) {
        float cosX = std::cos(euler.x * 0.5f);
        float sinX = std::sin(euler.x * 0.5f);
        float cosY = std::cos(euler.y * 0.5f);
        float sinY = std::sin(euler.y * 0.5f);
        float cosZ = std::cos(euler.z * 0.5f);
        float sinZ = std::sin(euler.z * 0.5f);

        Quaternion result;
        result.x = sinX * cosY * cosZ - cosX * sinY * sinZ;
        result.y = cosX * sinY * cosZ + sinX * cosY * sinZ;
        result.z = cosX * cosY * sinZ - sinX * sinY * cosZ;
        result.w = cosX * cosY * cosZ + sinX * sinY * sinZ;
        return result;
    }

    // クォータニオンをオイラー角へ戻す。エディタ表示やデバッグ確認に使う。
    Vector3 ToEuler(const Quaternion& quaternion) {
        Vector3 euler;

        float sinRollCosPitch = 2.0f * (quaternion.w * quaternion.x + quaternion.y * quaternion.z);
        float cosRollCosPitch = 1.0f - 2.0f * (quaternion.x * quaternion.x + quaternion.y * quaternion.y);
        euler.x = std::atan2(sinRollCosPitch, cosRollCosPitch);

        float sinPitch = 2.0f * (quaternion.w * quaternion.y - quaternion.z * quaternion.x);
        if (std::abs(sinPitch) >= 1.0f) {
            euler.y = std::copysign(3.14159265f / 2.0f, sinPitch);
        } else {
            euler.y = std::asin(sinPitch);
        }

        float sinYawCosPitch = 2.0f * (quaternion.w * quaternion.z + quaternion.x * quaternion.y);
        float cosYawCosPitch = 1.0f - 2.0f * (quaternion.y * quaternion.y + quaternion.z * quaternion.z);
        euler.z = std::atan2(sinYawCosPitch, cosYawCosPitch);

        return euler;
    }

}
