#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

// 小規模なデバッグ描画や簡易モデル描画に使う、共有モデルキャッシュ。
class ModelManager {
public:
    // 初期化。モデル生成に必要なObject3dCommonを保持し、内部描画用Object3dを作成する。
    static void Initialize(Object3dCommon* common);

    // 終了処理。キャッシュ済みモデルと内部描画インスタンスを破棄する。
    static void Finalize();

    // 指定モデルを読み込み済みキャッシュから取り出し、内部Object3dを使い回して1回描画する。
    static void Draw(
        const std::string& modelName,
        const Vector3& pos,
        const Vector3& rot = { 0,0,0 },
        const Vector3& scale = { 1,1,1 },
        const Vector4& color = { 1,1,1,1 },
		const Matrix4x4& lightVP = Math::MakeIdentity4x4()
    );

    // Draw前に使用するカメラ行列を設定する。
    static void SetCamera(const Matrix4x4& view, const Matrix4x4& projection);

private:
    static Object3dCommon* common_;
    // modelNameをキーにしたModelキャッシュ。重複読み込みを避ける。
    static std::unordered_map<std::string, std::unique_ptr<Model>> models_;
    static std::unique_ptr<Object3d> internalObject_; // 描画用の使い回しインスタンス。
    static Matrix4x4 viewMatrix_;
    static Matrix4x4 projectionMatrix_;
};

