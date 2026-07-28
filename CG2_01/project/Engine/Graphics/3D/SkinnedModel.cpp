#define NOMINMAX
#include "SkinnedModel.h"
#include "GltfLoader.h"
#include "MyMath.h"
#include <cassert>
#include <cmath>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "json.hpp"

using json = nlohmann::json;

namespace {
    // 座標を行列で変換し、w 成分で透視除算する。
    Vector3 TransformCoord(const Vector3& position, const Matrix4x4& matrix) {
        float homogeneousW =
            position.x * matrix.m[0][3] +
            position.y * matrix.m[1][3] +
            position.z * matrix.m[2][3] +
            matrix.m[3][3];
        if (std::abs(homogeneousW) < 1e-5f) {
            homogeneousW = 1.0f;
        }
        return {
            (position.x * matrix.m[0][0] + position.y * matrix.m[1][0] + position.z * matrix.m[2][0] + matrix.m[3][0]) / homogeneousW,
            (position.x * matrix.m[0][1] + position.y * matrix.m[1][1] + position.z * matrix.m[2][1] + matrix.m[3][1]) / homogeneousW,
            (position.x * matrix.m[0][2] + position.y * matrix.m[1][2] + position.z * matrix.m[2][2] + matrix.m[3][2]) / homogeneousW
        };
    }

    // 法線を行列の回転・スケール成分で変換し、単位ベクトルに戻す。
    Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix) {
        Vector3 transformedNormal = {
            normal.x * matrix.m[0][0] + normal.y * matrix.m[1][0] + normal.z * matrix.m[2][0],
            normal.x * matrix.m[0][1] + normal.y * matrix.m[1][1] + normal.z * matrix.m[2][1],
            normal.x * matrix.m[0][2] + normal.y * matrix.m[1][2] + normal.z * matrix.m[2][2]
        };
        return Math::Normalize(transformedNormal);
    }

    json ToJson(const Vector3& value) {
        return json::array({ value.x, value.y, value.z });
    }

    json ToJson(const Quaternion& value) {
        return json::array({ value.x, value.y, value.z, value.w });
    }

    Vector3 ReadVector3(const json& value, const Vector3& fallback) {
        if (!value.is_array() || value.size() < 3) {
            return fallback;
        }
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>()
        };
    }

    Quaternion ReadQuaternion(const json& value, const Quaternion& fallback) {
        if (!value.is_array() || value.size() < 4) {
            return fallback;
        }
        return {
            value.at(0).get<float>(),
            value.at(1).get<float>(),
            value.at(2).get<float>(),
            value.at(3).get<float>()
        };
    }

    void SortKeyframes(JointAnimation& animation) {
        std::sort(animation.keyframes.begin(), animation.keyframes.end(), [](const JointKeyframe& a, const JointKeyframe& b) {
            return a.time < b.time;
        });
    }

    float Clamp01(float value) {
        return std::max(0.0f, std::min(1.0f, value));
    }

    float LerpFloat(float start, float end, float rate) {
        return start + (end - start) * rate;
    }

    Vector3 LerpVector3(const Vector3& start, const Vector3& end, float rate) {
        return {
            LerpFloat(start.x, end.x, rate),
            LerpFloat(start.y, end.y, rate),
            LerpFloat(start.z, end.z, rate)
        };
    }

    JointKeyframe MakeRestKeyframe(const Joint& joint) {
        JointKeyframe pose;
        pose.translation = joint.restTranslation;
        pose.rotation = joint.restRotation;
        pose.scale = joint.restScale;
        pose.rotationQuat = joint.restRotationQuat;
        pose.isQuaternion = joint.restIsQuaternion;
        return pose;
    }

    JointKeyframe SampleJointAnimation(const JointAnimation& animation, const JointKeyframe& restPose, float loopedTime) {
        if (animation.keyframes.empty()) {
            return restPose;
        }
        if (animation.keyframes.size() == 1) {
            return animation.keyframes.front();
        }
        if (loopedTime <= animation.keyframes.front().time) {
            return animation.keyframes.front();
        }
        if (loopedTime >= animation.keyframes.back().time) {
            return animation.keyframes.back();
        }

        for (size_t keyIndex = 0; keyIndex + 1 < animation.keyframes.size(); ++keyIndex) {
            const auto& keyA = animation.keyframes[keyIndex];
            const auto& keyB = animation.keyframes[keyIndex + 1];
            if (loopedTime < keyA.time || loopedTime > keyB.time) {
                continue;
            }

            float range = keyB.time - keyA.time;
            float rate = range > 0.0f ? Clamp01((loopedTime - keyA.time) / range) : 0.0f;

            JointKeyframe pose;
            pose.time = loopedTime;
            pose.translation = LerpVector3(keyA.translation, keyB.translation, rate);
            pose.scale = LerpVector3(keyA.scale, keyB.scale, rate);
            if (keyA.isQuaternion && keyB.isQuaternion) {
                pose.rotationQuat = Math::Slerp(keyA.rotationQuat, keyB.rotationQuat, rate);
                pose.rotation = Math::ToEuler(pose.rotationQuat);
                pose.isQuaternion = true;
            } else {
                pose.rotation = LerpVector3(keyA.rotation, keyB.rotation, rate);
                pose.rotationQuat = Math::MakeQuaternionFromEuler(pose.rotation);
                pose.isQuaternion = false;
            }
            return pose;
        }

        return animation.keyframes.back();
    }

    std::vector<JointKeyframe> SampleMotionPose(const MotionData& motion, const std::vector<Joint>& joints, float time) {
        std::vector<JointKeyframe> pose(joints.size());
        float duration = std::max(0.001f, motion.duration);
        float loopedTime = std::fmod(time, duration);
        if (loopedTime < 0.0f) {
            loopedTime += duration;
        }

        for (size_t jointIndex = 0; jointIndex < joints.size(); ++jointIndex) {
            JointKeyframe restPose = MakeRestKeyframe(joints[jointIndex]);
            if (jointIndex >= motion.jointAnimations.size()) {
                pose[jointIndex] = restPose;
                continue;
            }
            pose[jointIndex] = SampleJointAnimation(motion.jointAnimations[jointIndex], restPose, loopedTime);
        }
        return pose;
    }

    bool MotionNameContainsAny(const MotionData& motion, const std::vector<std::string>& hints) {
        std::string name = motion.name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        for (const auto& hint : hints) {
            if (name.find(hint) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    int FindMotionIndexByHints(const std::vector<MotionData>& motions, const std::vector<std::string>& hints) {
        for (size_t i = 0; i < motions.size(); ++i) {
            if (MotionNameContainsAny(motions[i], hints)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool JointNameContainsAny(const Joint& joint, const std::vector<std::string>& hints) {
        std::string name = joint.name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        for (const auto& hint : hints) {
            if (name.find(hint) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    int FindJointIndexByHints(const std::vector<Joint>& joints, const std::vector<std::string>& hints) {
        for (size_t i = 0; i < joints.size(); ++i) {
            if (JointNameContainsAny(joints[i], hints)) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    MotionData CreateRestMotion(const std::vector<Joint>& joints, const std::string& name, float duration) {
        MotionData motion;
        motion.name = name;
        motion.duration = duration;
        motion.jointAnimations.resize(joints.size());

        for (size_t i = 0; i < joints.size(); ++i) {
            motion.jointAnimations[i].name = joints[i].name;
            JointKeyframe pose = MakeRestKeyframe(joints[i]);
            pose.time = 0.0f;
            motion.jointAnimations[i].keyframes.push_back(pose);

            pose.time = duration;
            motion.jointAnimations[i].keyframes.push_back(pose);
        }

        return motion;
    }

    void AddEulerOffset(MotionData& motion, int jointIndex, size_t keyIndex, const Vector3& offset) {
        if (jointIndex < 0 || jointIndex >= static_cast<int>(motion.jointAnimations.size())) {
            return;
        }
        auto& keyframes = motion.jointAnimations[static_cast<size_t>(jointIndex)].keyframes;
        if (keyIndex >= keyframes.size()) {
            return;
        }

        auto& pose = keyframes[keyIndex];
        pose.rotation.x += offset.x;
        pose.rotation.y += offset.y;
        pose.rotation.z += offset.z;
        pose.rotationQuat = Math::MakeQuaternionFromEuler(pose.rotation);
        pose.isQuaternion = false;
    }

    void AddTranslationOffset(MotionData& motion, int jointIndex, size_t keyIndex, const Vector3& offset) {
        if (jointIndex < 0 || jointIndex >= static_cast<int>(motion.jointAnimations.size())) {
            return;
        }
        auto& keyframes = motion.jointAnimations[static_cast<size_t>(jointIndex)].keyframes;
        if (keyIndex >= keyframes.size()) {
            return;
        }

        keyframes[keyIndex].translation.x += offset.x;
        keyframes[keyIndex].translation.y += offset.y;
        keyframes[keyIndex].translation.z += offset.z;
    }
}

SkinnedModel::~SkinnedModel() {
    if (jointBuffer_ && mappedPalette_) {
        jointBuffer_->Unmap(0, nullptr);
        mappedPalette_ = nullptr;
    }

    if (skinningInformationBuffer_ && mappedSkinningInformation_) {
        skinningInformationBuffer_->Unmap(0, nullptr);
        mappedSkinningInformation_ = nullptr;
    }
}

void SkinnedModel::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    restPoseCaptured_ = false;
    // 1. 
    CreateHumanoidSkeleton();

    // 2.  () 
    GenerateHumanoidMesh();
    
    // 3. 
    SmoothWeights();

    BuildJointMetadata();

    // 4. 
    if (textureManager) {
        textureHandle_ = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    }

    // 5. 
    CreateBuffers(dxCommon);

    // 6.  Model ()
    model_ = std::make_unique<Model>();
    // model_ 

    // 7.  ()
    ResetPose();
    CaptureRestPose();
}

void SkinnedModel::InitializeFromGltf(DirectXCommon* dxCommon, const std::string& filePath, TextureManager* textureManager) {
    restPoseCaptured_ = false;
    std::string texturePath;
    
    // glTF
    bool success = GltfLoader::LoadGltfModel(
        dxCommon,
        textureManager,
        filePath,
        skinnedVertices_,
        joints_,
        motions_,
        texturePath
    );

    if (!success) {
        OutputDebugStringA("Failed to load glTF model. Falling back to default humanoid mesh.\n");
        Initialize(dxCommon, textureManager);
        return;
    }

    activeMotionIndex_ = motions_.empty() ? -1 : 0;

    if (textureManager && !texturePath.empty()) {
        textureHandle_ = textureManager->LoadTexture(texturePath);
    }

    CreateBuffers(dxCommon);

    model_ = std::make_unique<Model>();

    BuildJointMetadata();
    CaptureRestPose();
}

void SkinnedModel::ResetPose() {
    if (restPoseCaptured_) {
        for (auto& joint : joints_) {
            joint.translation = joint.restTranslation;
            joint.rotation = joint.restRotation;
            joint.scale = joint.restScale;
            joint.rotationQuat = joint.restRotationQuat;
            joint.isQuaternion = joint.restIsQuaternion;
        }
        return;
    }

    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        

        if (joints_[i].name == "LeftShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, 1.3f };  // ()
        } else if (joints_[i].name == "RightShoulder") {
            joints_[i].rotation = { 0.0f, 0.0f, -1.3f }; // ()
        } else {
            joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        }
        joints_[i].rotationQuat = Math::MakeQuaternionFromEuler(joints_[i].rotation);
        joints_[i].isQuaternion = false;
    }
}

void SkinnedModel::CreateHumanoidSkeleton() {
    joints_.clear();

    // ()
    struct JointDef {
        std::string name;
        Vector3 globalPos;
        int parentIndex;
    };

    //  (5
    std::vector<JointDef> defs = {
        { "Pelvis",        { 0.0f, 0.8f, 0.0f },    -1 }, // 0
        { "Spine",         { 0.0f, 1.1f, 0.0f },     0 }, // 1
        { "Head",          { 0.0f, 1.4f, 0.0f },     1 }, // 2
        
        { "LeftShoulder",  { -0.25f, 1.3f, 0.0f },   1 }, // 3
        { "LeftElbow",     { -0.5f, 1.3f, 0.0f },    3 }, // 4
        { "LeftHand",      { -0.7f, 1.3f, 0.0f },    4 }, // 5
        
        { "RightShoulder", { 0.25f, 1.3f, 0.0f },    1 }, // 6
        { "RightElbow",    { 0.5f, 1.3f, 0.0f },     6 }, // 7
        { "RightHand",     { 0.7f, 1.3f, 0.0f },     7 }, // 8
        
        { "LeftHip",       { -0.15f, 0.7f, 0.0f },   0 }, // 9
        { "LeftKnee",      { -0.15f, 0.35f, 0.0f },  9 }, // 10
        { "LeftFoot",      { -0.15f, 0.0f, 0.0f },   10 }, // 11
        
        { "RightHip",      { 0.15f, 0.7f, 0.0f },    0 }, // 12
        { "RightKnee",     { 0.15f, 0.35f, 0.0f },  12 }, // 13
        { "RightFoot",     { 0.15f, 0.0f, 0.0f },   13 }  // 14
    };

    joints_.resize(defs.size());


    for (size_t i = 0; i < defs.size(); ++i) {
        joints_[i].name = defs[i].name;
        joints_[i].scale = { 1.0f, 1.0f, 1.0f };
        joints_[i].rotation = { 0.0f, 0.0f, 0.0f };
        joints_[i].parentIndex = defs[i].parentIndex;
        joints_[i].externalParentMatrix = Math::MakeIdentity4x4();

        // ( translation) 
        if (defs[i].parentIndex == -1) {
            joints_[i].translation = defs[i].globalPos;
        } else {
            int parentIdx = defs[i].parentIndex;
            joints_[i].translation = {
                defs[i].globalPos.x - defs[parentIdx].globalPos.x,
                defs[i].globalPos.y - defs[parentIdx].globalPos.y,
                defs[i].globalPos.z - defs[parentIdx].globalPos.z
            };
        }
    }


    for (size_t i = 0; i < joints_.size(); ++i) {
        Matrix4x4 localTrans = Math::MakeTranslateMatrix(joints_[i].translation);
        //  localMatrix 
        joints_[i].localMatrix = localTrans;

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }

        //  (Inverse Bind Pose Matrix)
        joints_[i].offsetMatrix = Math::Inverse(joints_[i].globalMatrix);
    }

    BuildJointMetadata();
}

void SkinnedModel::BuildJointMetadata() {
    rootJointIndex_ = -1;
    jointIndexMap_.clear();

    for (auto& joint : joints_) {
        joint.childIndices.clear();
    }

    for (size_t i = 0; i < joints_.size(); ++i) {
        Joint& joint = joints_[i];
        jointIndexMap_[joint.name] = static_cast<int>(i);

        if (joint.parentIndex >= 0 && joint.parentIndex < static_cast<int>(joints_.size())) {
            joints_[joint.parentIndex].childIndices.push_back(static_cast<int>(i));
        } else if (rootJointIndex_ == -1) {
            rootJointIndex_ = static_cast<int>(i);
        }
    }
}

void SkinnedModel::CaptureRestPose() {
    for (auto& joint : joints_) {
        joint.restTranslation = joint.translation;
        joint.restRotation = joint.rotation;
        joint.restScale = joint.scale;
        joint.restRotationQuat = joint.rotationQuat;
        joint.restIsQuaternion = joint.isQuaternion;
    }
    restPoseCaptured_ = true;
}

void SkinnedModel::GenerateHumanoidMesh() {
    skinnedVertices_.clear();

    //  Cube 
    // Pelvis ()
    AddCubeMesh({ 0.0f, 0.8f, 0.0f }, { 0.4f, 0.2f, 0.2f }, 0);
    // Spine ()
    AddCubeMesh({ 0.0f, 1.1f, 0.0f }, { 0.5f, 0.4f, 0.22f }, 1);
    // Head ()
    AddCubeMesh({ 0.0f, 1.45f, 0.0f }, { 0.2f, 0.2f, 0.2f }, 2);


    AddCubeMesh({ -0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 3);
    AddCubeMesh({ -0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 4);
    AddCubeMesh({ -0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 5);


    AddCubeMesh({ 0.375f, 1.3f, 0.0f }, { 0.25f, 0.1f, 0.1f }, 6);
    AddCubeMesh({ 0.6f, 1.3f, 0.0f }, { 0.2f, 0.08f, 0.08f }, 7);
    AddCubeMesh({ 0.725f, 1.3f, 0.0f }, { 0.07f, 0.07f, 0.07f }, 8);


    AddCubeMesh({ -0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 9);
    AddCubeMesh({ -0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 10);
    AddCubeMesh({ -0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 11);


    AddCubeMesh({ 0.15f, 0.525f, 0.0f }, { 0.12f, 0.35f, 0.12f }, 12);
    AddCubeMesh({ 0.15f, 0.175f, 0.0f }, { 0.1f, 0.35f, 0.1f }, 13);
    AddCubeMesh({ 0.15f, -0.05f, 0.05f }, { 0.1f, 0.1f, 0.15f }, 14);
}

void SkinnedModel::AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    Vector3 localVertices[8] = {
        { center.x - hx, center.y - hy, center.z - hz }, // 0
        { center.x + hx, center.y - hy, center.z - hz }, // 1
        { center.x - hx, center.y + hy, center.z - hz }, // 2
        { center.x + hx, center.y + hy, center.z - hz }, // 3
        { center.x - hx, center.y - hy, center.z + hz }, // 4
        { center.x + hx, center.y - hy, center.z + hz }, // 5
        { center.x - hx, center.y + hy, center.z + hz }, // 6
        { center.x + hx, center.y + hy, center.z + hz }  // 7
    };

    struct Face {
        int idx[4];
        Vector3 normal;
    };
    Face faces[6] = {
        { { 0, 2, 3, 1 }, { 0.0f, 0.0f, -1.0f } },
        { { 1, 3, 7, 5 }, { 1.0f, 0.0f, 0.0f } },
        { { 5, 7, 6, 4 }, { 0.0f, 0.0f, 1.0f } },
        { { 4, 6, 2, 0 }, { -1.0f, 0.0f, 0.0f } },
        { { 2, 6, 7, 3 }, { 0.0f, 1.0f, 0.0f } },
        { { 4, 0, 1, 5 }, { 0.0f, -1.0f, 0.0f } }
    };

    for (int f = 0; f < 6; ++f) {
        int indices[6] = {
            faces[f].idx[0], faces[f].idx[1], faces[f].idx[2],
            faces[f].idx[0], faces[f].idx[2], faces[f].idx[3]
        };

        Vector2 uvs[6] = {
            { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f },
            { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f }
        };

        for (int i = 0; i < 6; ++i) {
            SkinnedVertexData v;
            v.position = { localVertices[indices[i]].x, localVertices[indices[i]].y, localVertices[indices[i]].z, 1.0f };
            v.normal = faces[f].normal;
            v.texcoord = uvs[i];
            
            v.jointIndices[0] = jointIndex;
            v.jointIndices[1] = 0; v.jointIndices[2] = 0; v.jointIndices[3] = 0;
            v.weights[0] = 1.0f;
            v.weights[1] = 0.0f; v.weights[2] = 0.0f; v.weights[3] = 0.0f;

            skinnedVertices_.push_back(v);
        }
    }
}

void SkinnedModel::SmoothWeights() {
    for (auto& v : skinnedVertices_) {
        Vector3 pos = { v.position.x, v.position.y, v.position.z };
        int primaryJoint = v.jointIndices[0];

        if (primaryJoint == 1 && pos.y < 1.0f) {
            float dist = (pos.y - 0.8f) / 0.2f;
            float weightSpine = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 1; v.weights[0] = weightSpine;
            v.jointIndices[1] = 0; v.weights[1] = 1.0f - weightSpine;
        } else if (primaryJoint == 0 && pos.y > 0.85f) {
            float dist = (pos.y - 0.8f) / 0.1f;
            float weightSpine = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 0; v.weights[0] = 1.0f - weightSpine;
            v.jointIndices[1] = 1; v.weights[1] = weightSpine;
        }
    }
}

void SkinnedModel::Update(DirectXCommon* dxCommon) {
    (void)dxCommon;
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (joints_[i].isQuaternion) {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotationQuat, joints_[i].translation);
        } else {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotation, joints_[i].translation);
        }
        joints_[i].localMatrix = Math::Multiply(joints_[i].localMatrix, joints_[i].externalParentMatrix);

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }
    }
    
    if (mappedPalette_ && !joints_.empty()) {
        for (size_t i = 0; i < joints_.size(); ++i) {
            Matrix4x4 skeletonSpaceMatrix = Math::Multiply(joints_[i].offsetMatrix, joints_[i].globalMatrix);
            mappedPalette_[i].skeletonSpaceMatrix = skeletonSpaceMatrix;
            mappedPalette_[i].skeletonSpaceInverseTransposeMatrix = Math::Transpose(Math::Inverse(skeletonSpaceMatrix));
        }
    }
}

void SkinnedModel::DispatchSkinning(DirectXCommon* dxCommon) {
    if (!dxCommon || skinnedVertices_.empty()) {
        return;
    }

    if (!computeRootSignature_ || !computePipelineState_) {
        CreateComputeSkinningPipeline(dxCommon);
    }
    if (!computeRootSignature_ || !computePipelineState_ || !skinnedVertexBuffer_ ||
        !vertexBuffer_ || !influenceBuffer_ || !jointBuffer_ || !skinningInformationBuffer_) {
        return;
    }

    auto* commandList = dxCommon->GetCommandList();
    TransitionSkinnedVertexBuffer(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    commandList->SetComputeRootSignature(computeRootSignature_.Get());
    commandList->SetPipelineState(computePipelineState_.Get());
    commandList->SetComputeRootShaderResourceView(0, jointBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(1, vertexBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootShaderResourceView(2, influenceBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootUnorderedAccessView(3, skinnedVertexBuffer_->GetGPUVirtualAddress());
    commandList->SetComputeRootConstantBufferView(4, skinningInformationBuffer_->GetGPUVirtualAddress());

    constexpr UINT kNumThreads = 1024;
    UINT threadGroupCount = static_cast<UINT>((skinnedVertices_.size() + kNumThreads - 1) / kNumThreads);
    commandList->Dispatch(threadGroupCount, 1, 1);

    TransitionSkinnedVertexBuffer(commandList, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void SkinnedModel::CreateBuffers(DirectXCommon* dxCommon) {
    if (skinnedVertices_.empty()) {
        return;
    }

    if (jointBuffer_) {
        jointBuffer_->Unmap(0, nullptr);
        mappedPalette_ = nullptr;
    }
    if (skinningInformationBuffer_) {
        skinningInformationBuffer_->Unmap(0, nullptr);
        mappedSkinningInformation_ = nullptr;
    }

    auto device = dxCommon->GetDevice();
    UINT sizeVB = static_cast<UINT>(sizeof(ModelVertexData) * skinnedVertices_.size());
    UINT sizeInfluence = static_cast<UINT>(sizeof(VertexInfluence) * skinnedVertices_.size());

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeVB;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    if (SUCCEEDED(hr)) {
        ModelVertexData* vertMap = nullptr;
        vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertMap));
        for (size_t i = 0; i < skinnedVertices_.size(); ++i) {
            vertMap[i].position = skinnedVertices_[i].position;
            vertMap[i].texcoord = skinnedVertices_[i].texcoord;
            vertMap[i].normal = skinnedVertices_[i].normal;
        }
        vertexBuffer_->Unmap(0, nullptr);

        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeVB;
        vertexBufferView_.StrideInBytes = sizeof(ModelVertexData);
    }

    resDesc.Width = sizeInfluence;
    hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&influenceBuffer_));

    if (SUCCEEDED(hr)) {
        VertexInfluence* influenceMap = nullptr;
        influenceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&influenceMap));
        for (size_t i = 0; i < skinnedVertices_.size(); ++i) {
            for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
                influenceMap[i].weights[influenceIndex] = skinnedVertices_[i].weights[influenceIndex];
                influenceMap[i].jointIndices[influenceIndex] = skinnedVertices_[i].jointIndices[influenceIndex];
            }
        }
        influenceBuffer_->Unmap(0, nullptr);

        influenceBufferView_.BufferLocation = influenceBuffer_->GetGPUVirtualAddress();
        influenceBufferView_.SizeInBytes = sizeInfluence;
        influenceBufferView_.StrideInBytes = sizeof(VertexInfluence);
    }
    
    // Create matrix palette buffer.
    if (!joints_.empty()) {
        UINT sizeJoints = static_cast<UINT>(sizeof(WellForGPU) * joints_.size());
        resDesc.Width = sizeJoints;
        HRESULT paletteHr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&jointBuffer_));
        if (SUCCEEDED(paletteHr)) {
            jointBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette_));
            for (size_t i = 0; i < joints_.size(); ++i) {
                mappedPalette_[i].skeletonSpaceMatrix = Math::MakeIdentity4x4();
                mappedPalette_[i].skeletonSpaceInverseTransposeMatrix = Math::MakeIdentity4x4();
            }
        }
    }

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    defaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    defaultHeapProps.CreationNodeMask = 1;
    defaultHeapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC skinnedVertexDesc = resDesc;
    skinnedVertexDesc.Width = sizeVB;
    skinnedVertexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &skinnedVertexDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&skinnedVertexBuffer_));

    if (SUCCEEDED(hr)) {
        skinnedVertexBufferState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        skinnedVertexBufferView_.BufferLocation = skinnedVertexBuffer_->GetGPUVirtualAddress();
        skinnedVertexBufferView_.SizeInBytes = sizeVB;
        skinnedVertexBufferView_.StrideInBytes = sizeof(ModelVertexData);
    }

    D3D12_RESOURCE_DESC skinningInfoDesc = resDesc;
    skinningInfoDesc.Width = (sizeof(SkinningInformationForGPU) + 0xff) & ~0xff;
    skinningInfoDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &skinningInfoDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&skinningInformationBuffer_));

    if (SUCCEEDED(hr)) {
        skinningInformationBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedSkinningInformation_));
        mappedSkinningInformation_->numVertices = static_cast<uint32_t>(skinnedVertices_.size());
    }
    
    CreateComputeSkinningPipeline(dxCommon);
}

void SkinnedModel::CreateComputeSkinningPipeline(DirectXCommon* dxCommon) {
    if (!dxCommon || computeRootSignature_ || computePipelineState_) {
        return;
    }

    auto* device = dxCommon->GetDevice();

    D3D12_ROOT_PARAMETER rootParameters[5] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameters[1].Descriptor.ShaderRegister = 1;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameters[2].Descriptor.ShaderRegister = 2;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    rootParameters[3].Descriptor.ShaderRegister = 0;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].Descriptor.ShaderRegister = 0;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
        return;
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&computeRootSignature_));
    assert(SUCCEEDED(hr));

    auto csBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Skinning.CS.hlsl", L"cs_6_0");
    assert(csBlob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
    computePipelineStateDesc.pRootSignature = computeRootSignature_.Get();
    computePipelineStateDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

    hr = device->CreateComputePipelineState(&computePipelineStateDesc, IID_PPV_ARGS(&computePipelineState_));
    assert(SUCCEEDED(hr));
}

void SkinnedModel::TransitionSkinnedVertexBuffer(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter) {
    if (!commandList || !skinnedVertexBuffer_ || skinnedVertexBufferState_ == stateAfter) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = skinnedVertexBuffer_.Get();
    barrier.Transition.StateBefore = skinnedVertexBufferState_;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    skinnedVertexBufferState_ = stateAfter;
}

void SkinnedModel::ApplyTestAnimation(float time, float speed) {
    float t = time * speed;
    joints_[0].rotation.z = std::sin(t) * 0.02f;
    joints_[0].translation.y = 0.8f + std::sin(t * 2.0f) * 0.02f;
    joints_[1].rotation.y = std::sin(t) * 0.05f;
    joints_[9].rotation.x = std::sin(t) * 0.4f;
    joints_[12].rotation.x = -std::sin(t) * 0.4f;
    joints_[10].rotation.x = (std::sin(t + 1.5f) + 1.0f) * 0.3f; 
    joints_[13].rotation.x = (-std::sin(t + 1.5f) + 1.0f) * 0.3f;
    joints_[3].rotation.x = -std::sin(t) * 0.3f;
    joints_[6].rotation.x = std::sin(t) * 0.3f;
    joints_[4].rotation.x = (std::sin(t - 1.0f) - 1.0f) * 0.2f;
    joints_[7].rotation.x = (-std::sin(t - 1.0f) - 1.0f) * 0.2f;
}
void SkinnedModel::ApplyMotion(float time) {
    const auto& activeMotion = GetMotionData();
    if (activeMotion.jointAnimations.empty()) {
        return;
    }

    if (restPoseCaptured_) {
        for (auto& joint : joints_) {
            joint.translation = joint.restTranslation;
            joint.rotation = joint.restRotation;
            joint.scale = joint.restScale;
            joint.rotationQuat = joint.restRotationQuat;
            joint.isQuaternion = joint.restIsQuaternion;
        }
    }

    // Duration 
    float loopedTime = std::fmod(time, activeMotion.duration);
    if (loopedTime < 0.0f) loopedTime += activeMotion.duration;

    for (size_t i = 0; i < joints_.size(); ++i) {
        if (i >= activeMotion.jointAnimations.size()) {
            continue;
        }
        const auto& jointAnim = activeMotion.jointAnimations[i];
        if (jointAnim.keyframes.empty()) {
            continue;
        }

        auto& joint = joints_[i];

        // 1
        if (jointAnim.keyframes.size() == 1) {
            joint.translation = jointAnim.keyframes[0].translation;
            joint.rotation = jointAnim.keyframes[0].rotation;
            joint.scale = jointAnim.keyframes[0].scale;
            joint.rotationQuat = jointAnim.keyframes[0].rotationQuat;
            joint.isQuaternion = jointAnim.keyframes[0].isQuaternion;
            continue;
        }


        if (loopedTime <= jointAnim.keyframes.front().time) {
            const auto& first = jointAnim.keyframes.front();
            joint.translation = first.translation;
            joint.rotation = first.rotation;
            joint.scale = first.scale;
            joint.rotationQuat = first.rotationQuat;
            joint.isQuaternion = first.isQuaternion;
            continue;
        }


        if (loopedTime >= jointAnim.keyframes.back().time) {
            const auto& last = jointAnim.keyframes.back();
            joint.translation = last.translation;
            joint.rotation = last.rotation;
            joint.scale = last.scale;
            joint.rotationQuat = last.rotationQuat;
            joint.isQuaternion = last.isQuaternion;
            continue;
        }


        for (size_t k = 0; k < jointAnim.keyframes.size() - 1; ++k) {
            const auto& kfA = jointAnim.keyframes[k];
            const auto& kfB = jointAnim.keyframes[k + 1];

            if (loopedTime >= kfA.time && loopedTime <= kfB.time) {
                float t = (loopedTime - kfA.time) / (kfB.time - kfA.time);


                joint.translation = {
                    kfA.translation.x + t * (kfB.translation.x - kfA.translation.x),
                    kfA.translation.y + t * (kfB.translation.y - kfA.translation.y),
                    kfA.translation.z + t * (kfB.translation.z - kfA.translation.z)
                };


                if (kfA.isQuaternion && kfB.isQuaternion) {
                    joint.rotationQuat = Math::Slerp(kfA.rotationQuat, kfB.rotationQuat, t);
                    joint.rotation = Math::ToEuler(joint.rotationQuat);
                    joint.isQuaternion = true;
                } else {
                    //  ( Lerp)
                    joint.rotation = {
                        kfA.rotation.x + t * (kfB.rotation.x - kfA.rotation.x),
                        kfA.rotation.y + t * (kfB.rotation.y - kfA.rotation.y),
                        kfA.rotation.z + t * (kfB.rotation.z - kfA.rotation.z)
                    };
                    joint.isQuaternion = false;
                }

                joint.scale = {
                    kfA.scale.x + t * (kfB.scale.x - kfA.scale.x),
                    kfA.scale.y + t * (kfB.scale.y - kfA.scale.y),
                    kfA.scale.z + t * (kfB.scale.z - kfA.scale.z)
                };
                break;
            }
        }
    }
}

void SkinnedModel::ApplyMotionBlend(int fromMotionIndex, int toMotionIndex, float time, float blendRate) {
    if (motions_.empty()) {
        return;
    }
    if (fromMotionIndex < 0 || fromMotionIndex >= static_cast<int>(motions_.size()) ||
        toMotionIndex < 0 || toMotionIndex >= static_cast<int>(motions_.size())) {
        ApplyMotion(time);
        return;
    }

    float rate = Clamp01(blendRate);
    const auto fromPose = SampleMotionPose(motions_[static_cast<size_t>(fromMotionIndex)], joints_, time);
    const auto toPose = SampleMotionPose(motions_[static_cast<size_t>(toMotionIndex)], joints_, time);

    // Translation/Scale are linearly blended. Rotation uses Slerp after converting Euler poses to quaternions.
    for (size_t jointIndex = 0; jointIndex < joints_.size(); ++jointIndex) {
        const auto& poseA = fromPose[jointIndex];
        const auto& poseB = toPose[jointIndex];
        auto& joint = joints_[jointIndex];

        joint.translation = LerpVector3(poseA.translation, poseB.translation, rate);
        joint.scale = LerpVector3(poseA.scale, poseB.scale, rate);

        Quaternion rotA = poseA.isQuaternion ? poseA.rotationQuat : Math::MakeQuaternionFromEuler(poseA.rotation);
        Quaternion rotB = poseB.isQuaternion ? poseB.rotationQuat : Math::MakeQuaternionFromEuler(poseB.rotation);
        joint.rotationQuat = Math::Slerp(rotA, rotB, rate);
        joint.rotation = Math::ToEuler(joint.rotationQuat);
        joint.isQuaternion = true;
    }
}


void SkinnedModel::GenerateWalkPreset() {
    ClearKeyframes();

    // 2.0秒のアニメーションを0.1秒刻み(計21キーフレーム)で生成
    GetMotionData().duration = 2.0f;
    float step = 0.1f;

    // 一時的にポーズを退避
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 2.0f + 1e-4f; t += step) {
        // テスト用歩行アニメーションのロジックを適用してポーズを計算
        // 2.0秒で1サイクル(周期 2 * PI)にするため、スピードを調節する
        // スピード speed = PI (t = 2.0秒のとき、入力値 = 2.0 * PI となる)
        ApplyTestAnimation(t, 3.14159265f);

        // その瞬間のポーズをキーフレームとして記録
        AddKeyframe(t);
    }

    // 元のポーズ(初期状態)を復元
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

void SkinnedModel::GenerateRunPreset() {
    ClearKeyframes();

    // 小走り。1サイクル1.0秒の素早いループが綺麗
    GetMotionData().duration = 1.0f;
    float step = 0.05f; // 1.0秒間を0.05秒刻み(合計21キーフレーム)で生成

    // 一時的にポーズを退避
    std::vector<Vector3> origTrans(joints_.size());
    std::vector<Vector3> origRot(joints_.size());
    std::vector<Vector3> origScale(joints_.size());
    for (size_t i = 0; i < joints_.size(); ++i) {
        origTrans[i] = joints_[i].translation;
        origRot[i] = joints_[i].rotation;
        origScale[i] = joints_[i].scale;
    }

    for (float t = 0.0f; t <= 1.0f + 1e-4f; t += step) {
        // 1.0秒で1サイクル(周期 2 * PI)にするため、スピードは 2.0 * PI
        float angle = t * 2.0f * 3.14159265f;

        ResetPose(); // Tポーズからスタート

        // --- 小走りロジック ---
        // 1. 骨盤 (腰) の上下運動(走る際の弾み) と左右のひねり
        joints_[0].translation.y = 0.76f + std::abs(std::sin(angle * 2.0f)) * 0.06f;
        joints_[0].rotation.y = std::sin(angle) * 0.1f;
        joints_[0].rotation.z = std::sin(angle) * 0.05f;

        // 2. 脊椎 (胸) の前傾姿勢(走る時は前につんのめる)
        joints_[1].rotation.x = 0.18f; // 前傾
        joints_[1].rotation.y = -std::sin(angle) * 0.08f; // 上半身のひねり

        // 3. 足(太ももと膝)
        // 左太もも(9), 左膝(10) | 右太もも(12), 右膝(13)
        joints_[9].rotation.x = std::sin(angle) * 0.7f;
        joints_[12].rotation.x = -std::sin(angle) * 0.7f;

        // 膝は後ろにのみ大きく曲がる(走る時のキックと引きつけ)
        joints_[10].rotation.x = (std::sin(angle + 1.57f) + 1.0f) * 0.55f;
        joints_[13].rotation.x = (-std::sin(angle + 1.57f) + 1.0f) * 0.55f;

        // 4. 腕(肩と肘)
        // 左肩(3), 左肘(4) | 右肩(6), 右肘(7)
        // 腕を大きく前後に振り、肘は90度近くに固定したまま振る
        joints_[3].rotation.x = -std::sin(angle) * 0.7f;
        joints_[6].rotation.x = std::sin(angle) * 0.7f;

        // 肘は90度(約1.3ラジアン)曲げて固定気味にする
        joints_[4].rotation.z = -1.3f - std::sin(angle) * 0.15f;
        joints_[7].rotation.z = 1.3f + std::sin(angle) * 0.15f;

        // 首を少し前に向ける
        joints_[2].rotation.x = -0.1f;

        // その瞬間のポーズをキーフレームとして記録
        AddKeyframe(t);
    }

    // 元のポーズ(初期状態)を復元
    for (size_t i = 0; i < joints_.size(); ++i) {
        joints_[i].translation = origTrans[i];
        joints_[i].rotation = origRot[i];
        joints_[i].scale = origScale[i];
    }
}

void SkinnedModel::GenerateJumpPreset() {
    if (motions_.empty()) {
        motions_.push_back(MotionData{ "Jump", 0.8f, {} });
        activeMotionIndex_ = 0;
    }

    MotionData jumpMotion = CreateRestMotion(joints_, "Jump", 0.8f);
    const int hips = FindJointIndexByHints(joints_, { "hips", "pelvis" });
    const int spine = FindJointIndexByHints(joints_, { "spine" });
    const int head = FindJointIndexByHints(joints_, { "head" });
    const int leftArm = FindJointIndexByHints(joints_, { "leftarm", "left_arm", "leftshoulder", "left shoulder" });
    const int rightArm = FindJointIndexByHints(joints_, { "rightarm", "right_arm", "rightshoulder", "right shoulder" });
    const int leftForeArm = FindJointIndexByHints(joints_, { "leftforearm", "left_forearm", "leftelbow", "left elbow" });
    const int rightForeArm = FindJointIndexByHints(joints_, { "rightforearm", "right_forearm", "rightelbow", "right elbow" });
    const int leftUpLeg = FindJointIndexByHints(joints_, { "leftupleg", "left_up_leg", "lefthip", "left hip" });
    const int rightUpLeg = FindJointIndexByHints(joints_, { "rightupleg", "right_up_leg", "righthip", "right hip" });
    const int leftLeg = FindJointIndexByHints(joints_, { "leftleg", "left_leg", "leftknee", "left knee" });
    const int rightLeg = FindJointIndexByHints(joints_, { "rightleg", "right_leg", "rightknee", "right knee" });

    const float times[] = { 0.0f, 0.12f, 0.28f, 0.55f, 0.8f };
    for (auto& jointAnimation : jumpMotion.jointAnimations) {
        if (jointAnimation.keyframes.empty()) {
            continue;
        }

        const JointKeyframe rest = jointAnimation.keyframes.front();
        jointAnimation.keyframes.clear();
        for (float time : times) {
            JointKeyframe key = rest;
            key.time = time;
            jointAnimation.keyframes.push_back(key);
        }
    }

    // 0: 通常姿勢、1: 踏み込み、2: 蹴り上げ、3: 空中、4: 着地戻り。
    AddTranslationOffset(jumpMotion, hips, 1, { 0.0f, -0.08f, 0.0f });
    AddTranslationOffset(jumpMotion, hips, 2, { 0.0f, 0.10f, 0.0f });
    AddTranslationOffset(jumpMotion, hips, 3, { 0.0f, 0.04f, 0.0f });

    AddEulerOffset(jumpMotion, spine, 1, { 0.20f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, spine, 2, { -0.10f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, spine, 3, { 0.10f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, head, 1, { -0.06f, 0.0f, 0.0f });

    AddEulerOffset(jumpMotion, leftArm, 1, { -0.45f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightArm, 1, { -0.45f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, leftArm, 2, { 0.85f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightArm, 2, { 0.85f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, leftArm, 3, { 0.45f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightArm, 3, { 0.45f, 0.0f, 0.0f });

    AddEulerOffset(jumpMotion, leftForeArm, 2, { 0.35f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightForeArm, 2, { 0.35f, 0.0f, 0.0f });

    AddEulerOffset(jumpMotion, leftUpLeg, 1, { 0.55f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightUpLeg, 1, { 0.55f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, leftLeg, 1, { -0.55f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightLeg, 1, { -0.55f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, leftUpLeg, 3, { 0.22f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightUpLeg, 3, { 0.22f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, leftLeg, 3, { -0.25f, 0.0f, 0.0f });
    AddEulerOffset(jumpMotion, rightLeg, 3, { -0.25f, 0.0f, 0.0f });

    GetMotionData() = jumpMotion;
}

void SkinnedModel::EnsureDefaultPlayerMotions() {
    if (joints_.empty()) {
        return;
    }

    const int previousActiveMotion = activeMotionIndex_;

    if (motions_.size() == 1 &&
        FindMotionIndexByHints(motions_, { "idle", "wait", "stand", "walk", "run", "sprint", "jump", "air", "fall", "climb", "ladder" }) < 0) {
        motions_.front().name = "Walk";
    }

    if (FindMotionIndexByHints(motions_, { "idle", "wait", "stand" }) < 0) {
        motions_.push_back(CreateRestMotion(joints_, "Idle", 1.0f));
    }

    int walkIndex = FindMotionIndexByHints(motions_, { "walk" });
    if (walkIndex < 0 && !motions_.empty()) {
        walkIndex = 0;
    }

    if (FindMotionIndexByHints(motions_, { "run", "sprint" }) < 0 && walkIndex >= 0) {
        MotionData runMotion = motions_[static_cast<size_t>(walkIndex)];
        runMotion.name = "Run";
        const float sourceDuration = std::max(0.001f, runMotion.duration);
        const float targetDuration = std::max(0.35f, sourceDuration * 0.55f);
        const float timeScale = targetDuration / sourceDuration;
        runMotion.duration = targetDuration;
        for (auto& jointAnimation : runMotion.jointAnimations) {
            for (auto& keyframe : jointAnimation.keyframes) {
                keyframe.time *= timeScale;
            }
        }
        motions_.push_back(runMotion);
    }

    if (FindMotionIndexByHints(motions_, { "jump", "air", "fall" }) < 0) {
        motions_.push_back(MotionData{ "Jump", 0.8f, {} });
        activeMotionIndex_ = static_cast<int>(motions_.size()) - 1;
        GenerateJumpPreset();
    }

    if (previousActiveMotion >= 0 && previousActiveMotion < static_cast<int>(motions_.size())) {
        activeMotionIndex_ = previousActiveMotion;
    } else {
        int idleIndex = FindMotionIndexByHints(motions_, { "idle", "wait", "stand" });
        activeMotionIndex_ = idleIndex >= 0 ? idleIndex : 0;
    }
}

float SkinnedModel::GetMotionDuration() const {
    if (activeMotionIndex_ >= 0 && activeMotionIndex_ < static_cast<int>(motions_.size())) {
        return motions_[activeMotionIndex_].duration;
    }
    return 2.0f;
}

void SkinnedModel::SetMotionDuration(float duration) {
    GetMotionData().duration = duration;
}

void SkinnedModel::ClearKeyframes() {
    GetMotionData().jointAnimations.clear();
}

void SkinnedModel::AddKeyframe(float time) {
    auto& motionData = GetMotionData();
    
    if (motionData.jointAnimations.empty()) {
        motionData.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            motionData.jointAnimations[i].name = joints_[i].name;
        }
    }
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (i >= motionData.jointAnimations.size()) {
            motionData.jointAnimations.resize(joints_.size());
        }
        JointAnimation& jointAnimation = motionData.jointAnimations[i];
        if (jointAnimation.name.empty()) {
            jointAnimation.name = joints_[i].name;
        }

        JointKeyframe kf;
        kf.time = time;
        kf.translation = joints_[i].translation;
        kf.rotation = joints_[i].rotation;
        kf.scale = joints_[i].scale;
        kf.rotationQuat = joints_[i].rotationQuat;
        kf.isQuaternion = joints_[i].isQuaternion;

        auto existing = std::find_if(jointAnimation.keyframes.begin(), jointAnimation.keyframes.end(), [time](const JointKeyframe& item) {
            return std::abs(item.time - time) < 1.0e-4f;
        });
        if (existing != jointAnimation.keyframes.end()) {
            *existing = kf;
        } else {
            jointAnimation.keyframes.push_back(kf);
        }
        SortKeyframes(jointAnimation);
    }
}

bool SkinnedModel::SaveMotion(const std::string& filePath) {
    try {
        const MotionData& motionData = GetMotionData();

        std::filesystem::path outputPath(filePath);
        if (outputPath.has_parent_path()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }

        json root;
        root["version"] = 1;
        root["type"] = "CG2Motion";
        root["modelName"] = name_;
        root["motion"]["name"] = motionData.name;
        root["motion"]["duration"] = motionData.duration;
        root["motion"]["active"] = activeMotionIndex_;

        json jointsJson = json::array();
        for (const auto& joint : joints_) {
            jointsJson.push_back({
                { "name", joint.name },
                { "parent", joint.parentIndex }
            });
        }
        root["skeleton"]["joints"] = jointsJson;

        json animationsJson = json::array();
        for (const auto& jointAnimation : motionData.jointAnimations) {
            json jointJson;
            jointJson["name"] = jointAnimation.name;
            jointJson["keyframes"] = json::array();

            for (const auto& keyframe : jointAnimation.keyframes) {
                jointJson["keyframes"].push_back({
                    { "time", keyframe.time },
                    { "translation", ToJson(keyframe.translation) },
                    { "rotation", ToJson(keyframe.rotation) },
                    { "scale", ToJson(keyframe.scale) },
                    { "rotationQuat", ToJson(keyframe.rotationQuat) },
                    { "isQuaternion", keyframe.isQuaternion }
                });
            }
            animationsJson.push_back(jointJson);
        }
        root["motion"]["joints"] = animationsJson;

        std::ofstream file(filePath);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open motion file for write: " + filePath + "\n").c_str());
            return false;
        }

        file << root.dump(4);
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(("SaveMotion failed: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

bool SkinnedModel::LoadMotion(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            OutputDebugStringA(("Failed to open motion file for read: " + filePath + "\n").c_str());
            return false;
        }

        json root;
        file >> root;

        const json& motionJson = root.contains("motion") ? root.at("motion") : root;

        MotionData loadedMotion;
        loadedMotion.name = motionJson.value("name", std::filesystem::path(filePath).stem().string());
        loadedMotion.duration = motionJson.value("duration", 2.0f);
        loadedMotion.jointAnimations.resize(joints_.size());
        for (size_t i = 0; i < joints_.size(); ++i) {
            loadedMotion.jointAnimations[i].name = joints_[i].name;
        }

        if (motionJson.contains("joints") && motionJson.at("joints").is_array()) {
            for (const auto& jointJson : motionJson.at("joints")) {
                std::string jointName = jointJson.value("name", "");
                auto jointIt = jointIndexMap_.find(jointName);
                if (jointIt == jointIndexMap_.end()) {
                    continue;
                }

                int jointIndex = jointIt->second;
                if (jointIndex < 0 || jointIndex >= static_cast<int>(loadedMotion.jointAnimations.size())) {
                    continue;
                }

                JointAnimation& jointAnimation = loadedMotion.jointAnimations[static_cast<size_t>(jointIndex)];
                jointAnimation.name = jointName;
                jointAnimation.keyframes.clear();

                if (!jointJson.contains("keyframes") || !jointJson.at("keyframes").is_array()) {
                    continue;
                }

                for (const auto& keyframeJson : jointJson.at("keyframes")) {
                    JointKeyframe keyframe;
                    keyframe.time = keyframeJson.value("time", 0.0f);
                    keyframe.translation = ReadVector3(keyframeJson.value("translation", json::array()), joints_[static_cast<size_t>(jointIndex)].translation);
                    keyframe.rotation = ReadVector3(keyframeJson.value("rotation", json::array()), joints_[static_cast<size_t>(jointIndex)].rotation);
                    keyframe.scale = ReadVector3(keyframeJson.value("scale", json::array()), joints_[static_cast<size_t>(jointIndex)].scale);
                    keyframe.rotationQuat = ReadQuaternion(keyframeJson.value("rotationQuat", json::array()), joints_[static_cast<size_t>(jointIndex)].rotationQuat);
                    keyframe.isQuaternion = keyframeJson.value("isQuaternion", false);
                    jointAnimation.keyframes.push_back(keyframe);
                }
                SortKeyframes(jointAnimation);
            }
        }

        if (motions_.empty()) {
            motions_.push_back(loadedMotion);
            activeMotionIndex_ = 0;
        } else if (activeMotionIndex_ >= 0 && activeMotionIndex_ < static_cast<int>(motions_.size())) {
            motions_[static_cast<size_t>(activeMotionIndex_)] = loadedMotion;
        } else {
            motions_.push_back(loadedMotion);
            activeMotionIndex_ = static_cast<int>(motions_.size()) - 1;
        }

        ApplyMotion(0.0f);
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA(("LoadMotion failed: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

MotionData& SkinnedModel::GetMotionData() {
    if (motions_.empty()) {
        motions_.push_back(MotionData{"Motion_0", 2.0f, {}});
        activeMotionIndex_ = 0;
    }
    if (activeMotionIndex_ < 0 || activeMotionIndex_ >= static_cast<int>(motions_.size())) {
        activeMotionIndex_ = 0;
    }
    return motions_[activeMotionIndex_];
}

const MotionData& SkinnedModel::GetMotionData() const {
    static const MotionData emptyMotion{ "Motion_0", 2.0f, {} };
    if (motions_.empty()) {
        return emptyMotion;
    }
    if (activeMotionIndex_ < 0 || activeMotionIndex_ >= static_cast<int>(motions_.size())) {
        return motions_.front();
    }
    return motions_[static_cast<size_t>(activeMotionIndex_)];
}

void SkinnedModel::SetActiveMotionIndex(int index) {
    if (index >= 0 && index < static_cast<int>(motions_.size())) {
        activeMotionIndex_ = index;
    }
}

void SkinnedModel::SetActiveMotionName(const std::string& name) {
    GetMotionData().name = name.empty() ? "CustomMotion" : name;
}

void SkinnedModel::PlayAnimation(const std::string& animationName) {
    for (size_t i = 0; i < motions_.size(); ++i) {
        if (motions_[i].name == animationName) {
            activeMotionIndex_ = static_cast<int>(i);
            ApplyMotion(0.0f);
            return;
        }
    }
}

void SkinnedModel::EvaluateAnimation(float time) {
    ApplyMotion(time);
}
