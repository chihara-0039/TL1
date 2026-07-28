#pragma once

#include "LevelDataLoader.h"
#include "../Collision/WorldCollisionBox.h"

#include <memory>
#include <string>
#include <vector>

class DirectXCommon;
class Model;
class Object3d;
class Object3dCommon;
class TextureManager;

/// <summary>
/// Blenderから書き出したレベルを、通常ゲームで使用できる状態へ変換・保持するクラス。
///
/// 担当する処理:
/// - JSON/.sceneの読み込み
/// - 静的OBJの生成、更新、通常描画、影描画
/// - Blender BOXコライダーからゲーム用AABBへの変換
/// - プレイヤースポーン地点の抽出
///
/// PlayerやMyGameそのものは所有せず、読み込んだレベル情報だけを管理する。
/// </summary>
class BlenderRuntimeLevel {
public:
    BlenderRuntimeLevel();
    ~BlenderRuntimeLevel();

    /// <summary>モデル生成に必要なエンジン機能への参照を設定する。</summary>
    void Initialize(
        Object3dCommon* object3dCommon,
        DirectXCommon* dxCommon,
        TextureManager* textureManager);

    /// <summary>
    /// 指定レベルを読み込み、以前の配置物・コライダー・スポーン情報を置き換える。
    /// </summary>
    bool Load(const std::string& filePath);

    /// <summary>ゲーム用カメラとライト行列を各配置OBJへ反映する。</summary>
    void Update(const Matrix4x4& view, const Matrix4x4& projection, const Matrix4x4& lightViewProjection);

    /// <summary>読み込みに成功した有効なOBJを描画する。</summary>
    void Draw();

    /// <summary>読み込みに成功した有効なOBJをシャドウマップへ描画する。</summary>
    void DrawShadow(const Matrix4x4& lightViewProjection);

    /// <summary>プレイヤー物理へ渡す、Blender由来のワールドAABB一覧。</summary>
    const std::vector<WorldCollisionBox>& GetCollisionBoxes() const { return collisionBoxes_; }

    /// <summary>有効なプレイヤースポーン地点を読み込めたか。</summary>
    bool HasPlayerSpawn() const { return hasPlayerSpawn_; }

    /// <summary>プレイヤーの足元基準として扱うワールド座標。</summary>
    const Vector3& GetPlayerSpawn() const { return playerSpawn_; }

    /// <summary>デバッグUIやログへ表示する直近の読み込み結果。</summary>
    const std::string& GetStatus() const { return status_; }

private:
    /// <summary>静的OBJ一個分の描画リソースと変換情報。</summary>
    struct RuntimeObject {
        std::unique_ptr<Model> model;      ///< OBJから生成したモデル本体。
        std::unique_ptr<Object3d> object;  ///< ワールドへ描画するObject3d。
        Transform transform{};             ///< Blender座標変換・親子合成済みのTransform。
    };

    /// <summary>階層化されたレベルノードを再帰的に登録する。</summary>
    bool AppendObjectRecursive(const LevelObjectData& objectData);

    /// <summary>file_nameからResources/Models内の実在OBJを検索する。</summary>
    static std::string ResolveModelPath(const std::string& fileName);

    /// <summary>スポーン種別の表記揺れを吸収し、Player用か判定する。</summary>
    static bool IsPlayerSpawnType(std::string type);

    /// <summary>ローカルBOXを回転・拡縮込みのワールドAABBへ変換する。</summary>
    static WorldCollisionBox MakeWorldCollisionBox(
        const Transform& transform,
        const LevelColliderData& collider);

    Object3dCommon* object3dCommon_ = nullptr; ///< Object3d初期化元。所有しない。
    DirectXCommon* dxCommon_ = nullptr;         ///< Model生成元。所有しない。
    TextureManager* textureManager_ = nullptr; ///< テクスチャ管理元。所有しない。

    std::vector<RuntimeObject> objects_;             ///< 描画可能な静的OBJ一覧。
    std::vector<WorldCollisionBox> collisionBoxes_;  ///< プレイヤー物理用AABB一覧。
    Vector3 playerSpawn_{ 0.0f, 1.5f, 0.0f };        ///< Blender指定の足元座標。
    bool hasPlayerSpawn_ = false;                     ///< playerSpawn_が有効か。
    std::string status_ = "Blender level: not loaded"; ///< 直近の読み込み結果。
};
