#pragma once

#include "MyMath.h"
#include <string>
#include <vector>

// Blenderアドオンから出力されたBoxコライダー情報。
struct LevelColliderData {
    bool enabled = false;
    std::string type;
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    Vector3 size{ 2.0f, 2.0f, 2.0f };
};

// Blender上で配置したプレイヤー/敵などの出現地点情報。
struct LevelSpawnPointData {
    bool enabled = false;
    std::string type;
};

// レベルJSON内の1オブジェクト分のデータ。
struct LevelObjectData {
    std::string type;
    std::string name;
    std::string fileName;
    bool disabled = false;
    LevelColliderData collider;
    LevelSpawnPointData spawnPoint;
    // Blender軸変換と親子階層を適用済みのワールド変換。
    // 利用側はJSON階層を意識せずObject3dへそのまま渡せる。
    Transform transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
    std::vector<LevelObjectData> children;
};

// 1レベルファイル全体のデータ。
struct LevelData {
    std::string name;
    std::vector<LevelObjectData> objects;
};

// Blender由来のレベルJSONをエンジン側データへ変換するローダー。
class LevelDataLoader {
public:
    // JSONの読み込みと変換だけを行い、Object3d生成はシーン/エディタ側に委ねる。
    static bool Load(const std::string& filePath, LevelData& outLevelData, std::string* outStatus = nullptr);
};
