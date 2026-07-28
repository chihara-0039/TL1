#include "LevelDataLoader.h"

#include "json.hpp"

#include <cassert>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
using json = nlohmann::json;

constexpr float kDegreesToRadians = 3.1415926535f / 180.0f;

Vector3 ReadVector3Array(const json& value, const Vector3& fallback) {
    if (!value.is_array() || value.size() < 3) {
        return fallback;
    }

    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>()
    };
}

std::string Trim(const std::string& text) {
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

int CountIndentTabs(const std::string& line) {
    int count = 0;
    while (count < static_cast<int>(line.size()) && line[count] == '\t') {
        ++count;
    }
    return count;
}

Vector3 ConvertBlenderVector(const Vector3& value) {
    return { value.x, value.z, value.y };
}

Vector3 ConvertBlenderEulerDegrees(const Vector3& degrees) {
    const Vector3 radians = {
        degrees.x * kDegreesToRadians,
        degrees.y * kDegreesToRadians,
        degrees.z * kDegreesToRadians
    };
    return { -radians.x, -radians.z, radians.y };
}

Transform ReadBlenderTransform(const json& object) {
    Transform transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    if (!object.contains("transform") || !object.at("transform").is_object()) {
        return transform;
    }

    const json& source = object.at("transform");
    const Vector3 translation = ReadVector3Array(source.value("translation", json::array()), transform.translate);
    const Vector3 rotation = ReadVector3Array(source.value("rotation", json::array()), transform.rotate);
    const Vector3 scaling = ReadVector3Array(source.value("scaling", json::array()), transform.scale);

    // Blender is Z-up while this game uses Y-up.
    // Mapping follows the assignment slides:
    //   game X <- Blender X
    //   game Y <- Blender Z
    //   game Z <- Blender Y
    transform.translate = { translation.x, translation.z, translation.y };
    // PythonアドオンはBlenderのEuler角を「度」でJSONへ出力する。
    // Object3dは「ラジアン」を要求するため、軸入れ替えと同時に単位も変換する。
    transform.rotate = {
        -rotation.x * kDegreesToRadians,
        -rotation.z * kDegreesToRadians,
         rotation.y * kDegreesToRadians
    };
    transform.scale = { scaling.x, scaling.z, scaling.y };
    return transform;
}

LevelColliderData ReadColliderFromJson(const json& object) {
    LevelColliderData collider;
    if (!object.contains("collider")) {
        return collider;
    }

    if (object.at("collider").is_string()) {
        collider.enabled = true;
        collider.type = object.at("collider").get<std::string>();
    } else if (object.at("collider").is_object()) {
        const json& source = object.at("collider");
        collider.enabled = true;
        collider.type = source.value("type", "BOX");
        collider.center = ConvertBlenderVector(ReadVector3Array(source.value("center", json::array()), collider.center));
        collider.size = ConvertBlenderVector(ReadVector3Array(source.value("size", json::array()), collider.size));
    }

    return collider;
}

LevelSpawnPointData ReadSpawnPointFromJson(const json& object);

Vector3 TransformPoint(const Vector3& point, const Transform& transform) {
    const Matrix4x4 matrix = Math::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    return {
        point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
        point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
        point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2]
    };
}

Transform ComposeTransform(const Transform& parent, const Transform& local) {
    Transform world = {};

    // This engine stores Object3d transforms as separate scale / Euler rotation / translation values.
    // For level placement we keep that format and bake parent influence into the child at load time.
    world.scale = {
        parent.scale.x * local.scale.x,
        parent.scale.y * local.scale.y,
        parent.scale.z * local.scale.z
    };
    world.rotate = {
        parent.rotate.x + local.rotate.x,
        parent.rotate.y + local.rotate.y,
        parent.rotate.z + local.rotate.z
    };
    world.translate = TransformPoint(local.translate, parent);
    return world;
}

LevelObjectData ReadObjectRecursive(const json& object, const Transform& parentTransform) {
    assert(object.is_object());
    assert(object.contains("type"));

    LevelObjectData result;
    result.type = object.at("type").get<std::string>();
    result.name = object.value("name", result.type);
    result.fileName = object.value("file_name", "");
    result.disabled = object.value("disabled", false);
    result.collider = ReadColliderFromJson(object);
    result.spawnPoint = ReadSpawnPointFromJson(object);
    result.transform = ComposeTransform(parentTransform, ReadBlenderTransform(object));

    if (object.contains("children") && object.at("children").is_array()) {
        const json& children = object.at("children");
        result.children.reserve(children.size());
        for (const json& child : children) {
            if (!child.is_object() || !child.contains("type")) {
                continue;
            }
            result.children.push_back(ReadObjectRecursive(child, result.transform));
        }
    }

    return result;
}

bool ReadVector3FromLine(const std::string& line, const std::string& prefix, Vector3& outValue) {
    std::istringstream stream(line);
    std::string token;
    stream >> token;
    if (token != prefix) {
        return false;
    }

    return static_cast<bool>(stream >> outValue.x >> outValue.y >> outValue.z);
}

std::string ReadTextValueFromLine(const std::string& line, const std::string& prefix) {
    if (line.rfind(prefix + " ", 0) != 0) {
        return "";
    }

    return Trim(line.substr(prefix.size() + 1));
}

bool ReadBoolText(const std::string& value) {
    return value == "1" || value == "true" || value == "True" || value == "TRUE" ||
           value == "yes" || value == "Yes" || value == "ON" || value == "on";
}

LevelSpawnPointData ReadSpawnPointFromJson(const json& object) {
    LevelSpawnPointData spawnPoint;
    if (!object.contains("spawn_point")) {
        return spawnPoint;
    }

    if (object.at("spawn_point").is_string()) {
        spawnPoint.enabled = true;
        spawnPoint.type = object.at("spawn_point").get<std::string>();
    } else if (object.at("spawn_point").is_object()) {
        const json& source = object.at("spawn_point");
        spawnPoint.enabled = source.value("enabled", true);
        spawnPoint.type = source.value("type", "Player");
    } else if (object.at("spawn_point").is_boolean()) {
        spawnPoint.enabled = object.at("spawn_point").get<bool>();
        spawnPoint.type = "Player";
    }

    return spawnPoint;
}

LevelObjectData ReadSceneObjectRecursive(
    const std::vector<std::string>& lines,
    size_t& index,
    int level,
    const Transform& parentTransform) {

    LevelObjectData result;
    result.type = Trim(lines[index].substr(level));
    result.name = result.type;

    Transform localTransform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    ++index;
    while (index < lines.size()) {
        const int indent = CountIndentTabs(lines[index]);
        if (indent < level) {
            break;
        }
        if (indent > level) {
            break;
        }

        const std::string line = Trim(lines[index]);
        if (line == "END") {
            ++index;
            break;
        }

        Vector3 value{};
        if (ReadVector3FromLine(line, "T", value)) {
            localTransform.translate = ConvertBlenderVector(value);
        } else if (ReadVector3FromLine(line, "R", value)) {
            localTransform.rotate = ConvertBlenderEulerDegrees(value);
        } else if (ReadVector3FromLine(line, "S", value)) {
            localTransform.scale = ConvertBlenderVector(value);
        } else if (line.rfind("N ", 0) == 0) {
            result.fileName = ReadTextValueFromLine(line, "N");
            result.name = result.fileName.empty() ? result.name : result.fileName;
        } else if (line.rfind("C ", 0) == 0) {
            result.collider.enabled = true;
            result.collider.type = ReadTextValueFromLine(line, "C");
        } else if (ReadVector3FromLine(line, "CC", value)) {
            result.collider.center = ConvertBlenderVector(value);
            result.collider.enabled = true;
        } else if (ReadVector3FromLine(line, "CS", value)) {
            result.collider.size = ConvertBlenderVector(value);
            result.collider.enabled = true;
        } else if (line.rfind("D ", 0) == 0 || line.rfind("DISABLED ", 0) == 0) {
            const std::string rawValue = line.rfind("D ", 0) == 0
                ? ReadTextValueFromLine(line, "D")
                : ReadTextValueFromLine(line, "DISABLED");
            result.disabled = ReadBoolText(rawValue);
        } else if (line.rfind("SP ", 0) == 0 || line.rfind("SPAWN ", 0) == 0) {
            const std::string rawValue = line.rfind("SP ", 0) == 0
                ? ReadTextValueFromLine(line, "SP")
                : ReadTextValueFromLine(line, "SPAWN");
            result.spawnPoint.enabled = true;
            result.spawnPoint.type = rawValue.empty() ? "Player" : rawValue;
        }

        ++index;
    }

    result.transform = ComposeTransform(parentTransform, localTransform);

    while (index < lines.size()) {
        const int childIndent = CountIndentTabs(lines[index]);
        if (childIndent <= level) {
            break;
        }
        result.children.push_back(ReadSceneObjectRecursive(lines, index, childIndent, result.transform));
    }

    return result;
}

bool LoadSceneText(const std::string& filePath, LevelData& outLevelData, std::string* outStatus) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        if (outStatus) {
            *outStatus = "Scene load failed: cannot open file.";
        }
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!Trim(line).empty()) {
            lines.push_back(line);
        }
    }

    if (lines.empty() || Trim(lines.front()) != "SCENE") {
        if (outStatus) {
            *outStatus = "Scene load failed: invalid .scene header.";
        }
        return false;
    }

    outLevelData.name = std::filesystem::path(filePath).stem().string();
    const Transform rootTransform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    size_t index = 1;
    while (index < lines.size()) {
        const int level = CountIndentTabs(lines[index]);
        outLevelData.objects.push_back(ReadSceneObjectRecursive(lines, index, level, rootTransform));
    }

    if (outStatus) {
        *outStatus = "Scene loaded: " + filePath;
    }
    return true;
}
} // namespace

bool LevelDataLoader::Load(const std::string& filePath, LevelData& outLevelData, std::string* outStatus) {
    outLevelData = {};

    const std::filesystem::path path(filePath);
    if (path.extension() == ".scene") {
        return LoadSceneText(filePath, outLevelData, outStatus);
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            if (outStatus) {
                *outStatus = "Level load failed: cannot open file.";
            }
            return false;
        }

        json deserialized;
        file >> deserialized;

        if (!deserialized.is_object() ||
            !deserialized.contains("name") ||
            !deserialized.contains("objects") ||
            !deserialized.at("objects").is_array()) {
            if (outStatus) {
                *outStatus = "Level load failed: invalid level json format.";
            }
            return false;
        }

        outLevelData.name = deserialized.at("name").get<std::string>();
        const json& objects = deserialized.at("objects");
        outLevelData.objects.reserve(objects.size());
        const Transform rootTransform = {
            { 1.0f, 1.0f, 1.0f },
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f }
        };

        for (const json& object : objects) {
            if (!object.is_object() || !object.contains("type")) {
                if (outStatus) {
                    *outStatus = "Level load failed: object without type.";
                }
                return false;
            }
            outLevelData.objects.push_back(ReadObjectRecursive(object, rootTransform));
        }

        if (outStatus) {
            *outStatus = "Level loaded: " + filePath;
        }
        return true;
    } catch (const std::exception& e) {
        if (outStatus) {
            *outStatus = std::string("Level load failed: ") + e.what();
        }
        return false;
    }
}
