#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "GltfLoader.h"
#include "tiny_gltf.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace {
    // 右手系行列から左手系行列への変換
    // M_lh = Sz * M_rh * Sz
    Matrix4x4 ConvertMatrixRtL(const Matrix4x4& m) {
        Matrix4x4 result = m;
        // Z行とZ列、およびZ移動の符号を反転
        result.m[0][2] *= -1.0f;
        result.m[1][2] *= -1.0f;
        result.m[2][0] *= -1.0f;
        result.m[2][1] *= -1.0f;
        result.m[3][2] *= -1.0f;
        return result;
    }

    Joint MakeJointFromNode(const tinygltf::Node& node, const std::string& fallbackName) {
        Joint joint{};
        joint.name = node.name.empty() ? fallbackName : node.name;
        joint.parentIndex = -1;
        joint.translation = { 0.0f, 0.0f, 0.0f };
        joint.rotationQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
        joint.scale = { 1.0f, 1.0f, 1.0f };

        if (node.translation.size() == 3) {
            joint.translation.x = static_cast<float>(node.translation[0]);
            joint.translation.y = static_cast<float>(node.translation[1]);
            joint.translation.z = static_cast<float>(node.translation[2]);
        }
        if (node.rotation.size() == 4) {
            joint.rotationQuat.x = static_cast<float>(node.rotation[0]);
            joint.rotationQuat.y = static_cast<float>(node.rotation[1]);
            joint.rotationQuat.z = static_cast<float>(node.rotation[2]);
            joint.rotationQuat.w = static_cast<float>(node.rotation[3]);
        }
        if (node.scale.size() == 3) {
            joint.scale.x = static_cast<float>(node.scale[0]);
            joint.scale.y = static_cast<float>(node.scale[1]);
            joint.scale.z = static_cast<float>(node.scale[2]);
        }

        joint.translation.z *= -1.0f;
        joint.rotationQuat.x *= -1.0f;
        joint.rotationQuat.y *= -1.0f;
        joint.rotation = Math::ToEuler(joint.rotationQuat);
        joint.isQuaternion = true;
        joint.localMatrix = Math::MakeAffineMatrix(joint.scale, joint.rotationQuat, joint.translation);
        joint.globalMatrix = joint.localMatrix;
        joint.offsetMatrix = Math::MakeIdentity4x4();
        joint.externalParentMatrix = Math::MakeIdentity4x4();
        return joint;
    }

    int FindParentNodeIndex(const tinygltf::Model& model, int childNodeIndex) {
        for (int nodeIndex = 0; nodeIndex < static_cast<int>(model.nodes.size()); ++nodeIndex) {
            const auto& parentNode = model.nodes[static_cast<size_t>(nodeIndex)];
            if (std::find(parentNode.children.begin(), parentNode.children.end(), childNodeIndex) != parentNode.children.end()) {
                return nodeIndex;
            }
        }
        return -1;
    }

    bool IsSkinJointNode(const tinygltf::Skin& skin, int nodeIndex) {
        return std::find(skin.joints.begin(), skin.joints.end(), nodeIndex) != skin.joints.end();
    }

    Vector3 ReadNodeScale(const tinygltf::Node& node) {
        if (node.scale.size() == 3) {
            return {
                static_cast<float>(node.scale[0]),
                static_cast<float>(node.scale[1]),
                static_cast<float>(node.scale[2])
            };
        }
        return { 1.0f, 1.0f, 1.0f };
    }

    Vector3 ReadNodeTranslation(const tinygltf::Node& node) {
        if (node.translation.size() == 3) {
            return {
                static_cast<float>(node.translation[0]),
                static_cast<float>(node.translation[1]),
                static_cast<float>(node.translation[2]) * -1.0f
            };
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    Quaternion NormalizeQuaternion(const Quaternion& q) {
        float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (length <= 0.0f) {
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        }
        return { q.x / length, q.y / length, q.z / length, q.w / length };
    }

    Quaternion ReadNodeRotation(const tinygltf::Node& node) {
        Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (node.rotation.size() == 4) {
            rotation.x = static_cast<float>(node.rotation[0]);
            rotation.y = static_cast<float>(node.rotation[1]);
            rotation.z = static_cast<float>(node.rotation[2]);
            rotation.w = static_cast<float>(node.rotation[3]);
            rotation.x *= -1.0f;
            rotation.y *= -1.0f;
        }
        return NormalizeQuaternion(rotation);
    }

    Matrix4x4 MakeNodeLocalMatrixRtL(const tinygltf::Node& node) {
        return Math::MakeAffineMatrix(ReadNodeScale(node), ReadNodeRotation(node), ReadNodeTranslation(node));
    }

    Vector3 GetExternalParentScale(const tinygltf::Model& model, const tinygltf::Skin& skin, int jointNodeIndex) {
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
        int parentNodeIndex = FindParentNodeIndex(model, jointNodeIndex);
        while (parentNodeIndex >= 0 && !IsSkinJointNode(skin, parentNodeIndex)) {
            Vector3 parentScale = ReadNodeScale(model.nodes[static_cast<size_t>(parentNodeIndex)]);
            scale.x *= parentScale.x;
            scale.y *= parentScale.y;
            scale.z *= parentScale.z;
            parentNodeIndex = FindParentNodeIndex(model, parentNodeIndex);
        }
        return scale;
    }

    Quaternion GetExternalParentRotation(const tinygltf::Model& model, const tinygltf::Skin& skin, int jointNodeIndex) {
        Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        int parentNodeIndex = FindParentNodeIndex(model, jointNodeIndex);
        while (parentNodeIndex >= 0 && !IsSkinJointNode(skin, parentNodeIndex)) {
            Quaternion parentRotation = ReadNodeRotation(model.nodes[static_cast<size_t>(parentNodeIndex)]);
            rotation = NormalizeQuaternion(Math::Multiply(rotation, parentRotation));
            parentNodeIndex = FindParentNodeIndex(model, parentNodeIndex);
        }
        return rotation;
    }

    Matrix4x4 GetExternalParentMatrix(const tinygltf::Model& model, const tinygltf::Skin& skin, int jointNodeIndex) {
        Matrix4x4 matrix = Math::MakeIdentity4x4();
        int parentNodeIndex = FindParentNodeIndex(model, jointNodeIndex);
        while (parentNodeIndex >= 0 && !IsSkinJointNode(skin, parentNodeIndex)) {
            Matrix4x4 parentMatrix = MakeNodeLocalMatrixRtL(model.nodes[static_cast<size_t>(parentNodeIndex)]);
            matrix = Math::Multiply(matrix, parentMatrix);
            parentNodeIndex = FindParentNodeIndex(model, parentNodeIndex);
        }
        return matrix;
    }

    bool HasExternalScale(const Vector3& scale) {
        return std::abs(scale.x - 1.0f) > 1.0e-5f ||
               std::abs(scale.y - 1.0f) > 1.0e-5f ||
               std::abs(scale.z - 1.0f) > 1.0e-5f;
    }

    bool HasExternalRotation(const Quaternion& rotation) {
        return std::abs(rotation.x) > 1.0e-5f ||
               std::abs(rotation.y) > 1.0e-5f ||
               std::abs(rotation.z) > 1.0e-5f ||
               std::abs(rotation.w - 1.0f) > 1.0e-5f;
    }
}

bool GltfLoader::LoadGltfModel(
    DirectXCommon* dxCommon,
    TextureManager* textureManager,
    const std::string& filePath,
    std::vector<SkinnedVertexData>& outVertices,
    std::vector<Joint>& outJoints,
    std::vector<MotionData>& outMotions,
    std::string& outTexturePath
) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();

    if (ext == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
    } else {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filePath);
    }

    if (!warn.empty()) {
        OutputDebugStringA(("GltfLoader Warn: " + warn + "\n").c_str());
    }
    if (!err.empty()) {
        OutputDebugStringA(("GltfLoader Error: " + err + "\n").c_str());
    }

    if (!ret) {
        OutputDebugStringA(("GltfLoader: Failed to load glTF file: " + filePath + "\n").c_str());
        return false;
    }

    if (model.meshes.empty()) {
        OutputDebugStringA("GltfLoader: No meshes found in glTF file.\n");
        return false;
    }

    // 1. テクスチャパスの取得
    outTexturePath = "Resources/uvChecker.png"; // デフォルト
    if (!model.images.empty()) {
        std::string imgUri = model.images[0].uri;
        if (!imgUri.empty()) {
            std::filesystem::path modelDir = path.parent_path();
            outTexturePath = (modelDir / imgUri).string();
            // バックスラッシュをスラッシュに置換
            std::replace(outTexturePath.begin(), outTexturePath.end(), '\\', '/');
        }
    } else if (!model.materials.empty()) {
        // マテリアルからテクスチャを探す
        const auto& mat = model.materials[0];
        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (texIdx >= 0 && texIdx < static_cast<int>(model.textures.size())) {
            int imgIdx = model.textures[texIdx].source;
            if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size())) {
                std::string imgUri = model.images[imgIdx].uri;
                if (!imgUri.empty()) {
                    std::filesystem::path modelDir = path.parent_path();
                    outTexturePath = (modelDir / imgUri).string();
                    std::replace(outTexturePath.begin(), outTexturePath.end(), '\\', '/');
                }
            }
        }
    }

    // 2. ジョイント（スケルトン）の構築
    outJoints.clear();
    if (!model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];
        outJoints.resize(skin.joints.size());

        // 各ジョイントノードの情報パース
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int nodeIdx = skin.joints[i];
            const tinygltf::Node& node = model.nodes[nodeIdx];
            Joint& joint = outJoints[i];

            joint.name = node.name.empty() ? ("Joint_" + std::to_string(i)) : node.name;

            // 初期トランスフォーム
            joint.translation = { 0.0f, 0.0f, 0.0f };
            Quaternion rotQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
            joint.scale = { 1.0f, 1.0f, 1.0f };

            if (node.translation.size() == 3) {
                joint.translation.x = static_cast<float>(node.translation[0]);
                joint.translation.y = static_cast<float>(node.translation[1]);
                joint.translation.z = static_cast<float>(node.translation[2]);
            }
            if (node.rotation.size() == 4) {
                rotQuat.x = static_cast<float>(node.rotation[0]);
                rotQuat.y = static_cast<float>(node.rotation[1]);
                rotQuat.z = static_cast<float>(node.rotation[2]);
                rotQuat.w = static_cast<float>(node.rotation[3]);
            }
            if (node.scale.size() == 3) {
                joint.scale.x = static_cast<float>(node.scale[0]);
                joint.scale.y = static_cast<float>(node.scale[1]);
                joint.scale.z = static_cast<float>(node.scale[2]);
            }

            // 右手系から左手系へ変換（Z軸反転）
            joint.translation.z *= -1.0f;
            rotQuat.x *= -1.0f;
            rotQuat.y *= -1.0f;

            joint.rotation = Math::ToEuler(rotQuat);
            joint.rotationQuat = rotQuat;
            joint.isQuaternion = true;
            joint.localMatrix = Math::MakeAffineMatrix(joint.scale, rotQuat, joint.translation);
            joint.externalParentMatrix = GetExternalParentMatrix(model, skin, nodeIdx);

            // 親インデックスの検索
            joint.parentIndex = -1;
            for (int p = 0; p < static_cast<int>(model.nodes.size()); ++p) {
                const auto& parentNode = model.nodes[p];
                auto childIt = std::find(parentNode.children.begin(), parentNode.children.end(), nodeIdx);
                if (childIt != parentNode.children.end()) {
                    auto it = std::find(skin.joints.begin(), skin.joints.end(), p);
                    if (it != skin.joints.end()) {
                        joint.parentIndex = static_cast<int>(std::distance(skin.joints.begin(), it));
                    }
                    break;
                }
            }
        }

        // 逆バインド行列の読み込み
        if (skin.inverseBindMatrices != -1) {
            const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            const float* matrixData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            for (size_t i = 0; i < skin.joints.size(); ++i) {
                Matrix4x4 rawOffsetMatrix;
                std::copy(matrixData + i * 16, matrixData + (i + 1) * 16, &rawOffsetMatrix.m[0][0]);
                // 右手系から左手系へ変換
                outJoints[i].offsetMatrix = ConvertMatrixRtL(rawOffsetMatrix);
            }
        }
    } else {
        int animatedNodeIndex = -1;
        for (const auto& anim : model.animations) {
            for (const auto& channel : anim.channels) {
                if (channel.target_node >= 0) {
                    animatedNodeIndex = channel.target_node;
                    break;
                }
            }
            if (animatedNodeIndex >= 0) {
                break;
            }
        }

        if (animatedNodeIndex < 0) {
            for (size_t i = 0; i < model.nodes.size(); ++i) {
                if (model.nodes[i].mesh >= 0) {
                    animatedNodeIndex = static_cast<int>(i);
                    break;
                }
            }
        }

        if (animatedNodeIndex >= 0 && animatedNodeIndex < static_cast<int>(model.nodes.size())) {
            outJoints.push_back(MakeJointFromNode(model.nodes[animatedNodeIndex], "NodeRoot"));
        } else {
            Joint root{};
            root.name = "Root";
            root.parentIndex = -1;
            root.translation = { 0.0f, 0.0f, 0.0f };
            root.rotation = { 0.0f, 0.0f, 0.0f };
            root.rotationQuat = { 0.0f, 0.0f, 0.0f, 1.0f };
            root.scale = { 1.0f, 1.0f, 1.0f };
            root.isQuaternion = true;
            root.localMatrix = Math::MakeIdentity4x4();
            root.globalMatrix = Math::MakeIdentity4x4();
            root.offsetMatrix = Math::MakeIdentity4x4();
            outJoints.push_back(root);
        }
    }

    // 3. メッシュ（頂点・インデックス・スキンウェイト）のパース
    outVertices.clear();

    for (const auto& mesh : model.meshes) {
        for (const auto& prim : mesh.primitives) {

            // アクセッサからのデータ抽出ヘルパー
            auto getFloatAttribute = [&](const std::string& name, std::vector<float>& outVec) {
                auto it = prim.attributes.find(name);
                if (it == prim.attributes.end()) return;

                const tinygltf::Accessor& accessor = model.accessors[it->second];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                int stride = accessor.ByteStride(bufferView);
                int numComp = tinygltf::GetNumComponentsInType(accessor.type);

                outVec.resize(accessor.count * numComp);
                const unsigned char* dataPtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];
                for (size_t i = 0; i < accessor.count; ++i) {
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        const float* ptr = reinterpret_cast<const float*>(dataPtr + i * stride);
                        for (int c = 0; c < numComp; ++c) {
                            outVec[i * numComp + c] = ptr[c];
                        }
                    }
                }
            };

            std::vector<float> posData, normData, uvData, weightsData;
            std::vector<uint32_t> jointIndicesData;

            getFloatAttribute("POSITION", posData);
            getFloatAttribute("NORMAL", normData);
            getFloatAttribute("TEXCOORD_0", uvData);

            // JOINTS_0
            auto itJoints = prim.attributes.find("JOINTS_0");
            if (itJoints != prim.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[itJoints->second];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                int stride = accessor.ByteStride(bufferView);
                const unsigned char* dataPtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

                jointIndicesData.resize(accessor.count * 4);
                for (size_t i = 0; i < accessor.count; ++i) {
                    const unsigned char* elementPtr = dataPtr + i * stride;
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* ptr = reinterpret_cast<const uint16_t*>(elementPtr);
                        for (int c = 0; c < 4; ++c) jointIndicesData[i * 4 + c] = ptr[c];
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(elementPtr);
                        for (int c = 0; c < 4; ++c) jointIndicesData[i * 4 + c] = ptr[c];
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(elementPtr);
                        for (int c = 0; c < 4; ++c) jointIndicesData[i * 4 + c] = ptr[c];
                    }
                }
            }

            // WEIGHTS_0
            getFloatAttribute("WEIGHTS_0", weightsData);

            // インデックスバッファのパース
            std::vector<uint32_t> indices;
            if (prim.indices != -1) {
                const tinygltf::Accessor& accessor = model.accessors[prim.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const unsigned char* dataPtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

                indices.resize(accessor.count);
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* p = reinterpret_cast<const uint32_t*>(dataPtr);
                    std::copy(p, p + accessor.count, indices.begin());
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* p = reinterpret_cast<const uint16_t*>(dataPtr);
                    for (size_t i = 0; i < accessor.count; ++i) indices[i] = p[i];
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* p = reinterpret_cast<const uint8_t*>(dataPtr);
                    for (size_t i = 0; i < accessor.count; ++i) indices[i] = p[i];
                }
            } else {
                // インデックスがない場合はシーケンシャルなインデックスを作成
                size_t count = posData.size() / 3;
                indices.resize(count);
                for (size_t i = 0; i < count; ++i) indices[i] = static_cast<uint32_t>(i);
            }

            // インデックスバッファを走査してインデックスなし頂点配列を作成（巻順を反転）
            size_t numTriangles = indices.size() / 3;
            for (size_t t = 0; t < numTriangles; ++t) {
                // 巻順反転
                uint32_t idx[3] = {
                    indices[t * 3 + 0],
                    indices[t * 3 + 2],
                    indices[t * 3 + 1]
                };

                for (int i = 0; i < 3; ++i) {
                    uint32_t vIdx = idx[i];

                    SkinnedVertexData v{};
                    v.position = { posData[vIdx * 3 + 0], posData[vIdx * 3 + 1], posData[vIdx * 3 + 2], 1.0f };
                    // 右手系から左手系へ変換（Z反転）
                    v.position.z *= -1.0f;

                    if (!normData.empty()) {
                        v.normal = { normData[vIdx * 3 + 0], normData[vIdx * 3 + 1], normData[vIdx * 3 + 2] };
                        v.normal.z *= -1.0f;
                    }

                    if (!uvData.empty()) {
                        v.texcoord = { uvData[vIdx * 2 + 0], 1.0f - uvData[vIdx * 2 + 1] };
                    }

                    if (!jointIndicesData.empty() && !weightsData.empty()) {
                        float wSum = 0.0f;
                        for (int c = 0; c < 4; ++c) {
                            v.jointIndices[c] = static_cast<int>(jointIndicesData[vIdx * 4 + c]);
                            v.weights[c] = weightsData[vIdx * 4 + c];
                            wSum += v.weights[c];
                        }
                        // ウェイト正規化
                        if (wSum > 0.0f) {
                            for (int c = 0; c < 4; ++c) v.weights[c] /= wSum;
                        }
                    } else {
                        v.jointIndices[0] = 0; v.jointIndices[1] = 0; v.jointIndices[2] = 0; v.jointIndices[3] = 0;
                        v.weights[0] = 1.0f; v.weights[1] = 0.0f; v.weights[2] = 0.0f; v.weights[3] = 0.0f;
                    }

                    outVertices.push_back(v);
                }
            }
        } // end for prim
    } // end for mesh

    // 4. アニメーションのパース
    outMotions.clear();

    if (!model.animations.empty() && !model.skins.empty()) {
        const tinygltf::Skin& skin = model.skins[0];

        for (size_t aIdx = 0; aIdx < model.animations.size(); ++aIdx) {
            const tinygltf::Animation& anim = model.animations[aIdx];
            MotionData motionData;
            motionData.name = anim.name.empty() ? ("Animation_" + std::to_string(aIdx)) : anim.name;
            motionData.duration = 0.0f;
            motionData.jointAnimations.resize(skin.joints.size());

            for (size_t i = 0; i < skin.joints.size(); ++i) {
                motionData.jointAnimations[i].name = outJoints[i].name;
            }

            // 各ボーンのアニメーションをマッピング
            for (const auto& channel : anim.channels) {
                int nodeIdx = channel.target_node;
                auto it = std::find(skin.joints.begin(), skin.joints.end(), nodeIdx);
                if (it == skin.joints.end()) continue;

                int jointIdx = static_cast<int>(std::distance(skin.joints.begin(), it));
                auto& jointAnim = motionData.jointAnimations[jointIdx];

                const tinygltf::AnimationSampler& sampler = anim.samplers[channel.sampler];

                // 時間アクセッサ
                const tinygltf::Accessor& timeAccessor = model.accessors[sampler.input];
                const tinygltf::BufferView& timeBufferView = model.bufferViews[timeAccessor.bufferView];
                const tinygltf::Buffer& timeBuffer = model.buffers[timeBufferView.buffer];
                const float* times = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);

                // 最大アニメーション時間更新
                if (timeAccessor.count > 0) {
                    motionData.duration = (std::max)(motionData.duration, times[timeAccessor.count - 1]);
                }

                // 値アクセッサ
                const tinygltf::Accessor& valueAccessor = model.accessors[sampler.output];
                const tinygltf::BufferView& valueBufferView = model.bufferViews[valueAccessor.bufferView];
                const tinygltf::Buffer& valueBuffer = model.buffers[valueBufferView.buffer];
                const float* values = reinterpret_cast<const float*>(&valueBuffer.data[valueBufferView.byteOffset + valueAccessor.byteOffset]);

                for (size_t k = 0; k < timeAccessor.count; ++k) {
                    float time = times[k];

                    // 該当時間のキーフレームを探すか追加
                    JointKeyframe* kf = nullptr;
                    auto kfIt = std::find_if(jointAnim.keyframes.begin(), jointAnim.keyframes.end(), [&](const JointKeyframe& k) {
                        return std::abs(k.time - time) < 1e-4f;
                    });

                    if (kfIt != jointAnim.keyframes.end()) {
                        kf = &(*kfIt);
                    } else {
                        JointKeyframe newKf;
                        newKf.time = time;
                        // デフォルト初期化
                        newKf.translation = outJoints[jointIdx].translation;
                        newKf.rotation = outJoints[jointIdx].rotation;
                        newKf.rotationQuat = outJoints[jointIdx].rotationQuat;
                        newKf.isQuaternion = true;
                        newKf.scale = outJoints[jointIdx].scale;
                        jointAnim.keyframes.push_back(newKf);
                        kf = &jointAnim.keyframes.back();
                    }

                    if (channel.target_path == "translation") {
                        kf->translation.x = values[k * 3 + 0];
                        kf->translation.y = values[k * 3 + 1];
                        kf->translation.z = values[k * 3 + 2];
                        // Z反転
                        kf->translation.z *= -1.0f;
                    } else if (channel.target_path == "rotation") {
                        kf->rotationQuat.x = values[k * 4 + 0];
                        kf->rotationQuat.y = values[k * 4 + 1];
                        kf->rotationQuat.z = values[k * 4 + 2];
                        kf->rotationQuat.w = values[k * 4 + 3];
                        // Z反転
                        kf->rotationQuat.x *= -1.0f;
                        kf->rotationQuat.y *= -1.0f;
                        kf->rotation = Math::ToEuler(kf->rotationQuat);
                        kf->isQuaternion = true;
                    } else if (channel.target_path == "scale") {
                        kf->scale.x = values[k * 3 + 0];
                        kf->scale.y = values[k * 3 + 1];
                        kf->scale.z = values[k * 3 + 2];
                    }
                }

                // 時間順にキーフレームをソート
                std::sort(jointAnim.keyframes.begin(), jointAnim.keyframes.end(), [](const JointKeyframe& a, const JointKeyframe& b) {
                    return a.time < b.time;
                });
            }
            outMotions.push_back(motionData);
        }
    } else if (!model.animations.empty() && model.skins.empty() && !outJoints.empty()) {
        for (size_t aIdx = 0; aIdx < model.animations.size(); ++aIdx) {
            const tinygltf::Animation& anim = model.animations[aIdx];
            MotionData motionData;
            motionData.name = anim.name.empty() ? ("NodeAnimation_" + std::to_string(aIdx)) : anim.name;
            motionData.duration = 0.0f;
            motionData.jointAnimations.resize(1);
            motionData.jointAnimations[0].name = outJoints[0].name;

            auto& jointAnim = motionData.jointAnimations[0];

            for (const auto& channel : anim.channels) {
                if (channel.target_node < 0) {
                    continue;
                }

                const tinygltf::AnimationSampler& sampler = anim.samplers[channel.sampler];
                const tinygltf::Accessor& timeAccessor = model.accessors[sampler.input];
                const tinygltf::BufferView& timeBufferView = model.bufferViews[timeAccessor.bufferView];
                const tinygltf::Buffer& timeBuffer = model.buffers[timeBufferView.buffer];
                const float* times = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);

                if (timeAccessor.count > 0) {
                    motionData.duration = (std::max)(motionData.duration, times[timeAccessor.count - 1]);
                }

                const tinygltf::Accessor& valueAccessor = model.accessors[sampler.output];
                const tinygltf::BufferView& valueBufferView = model.bufferViews[valueAccessor.bufferView];
                const tinygltf::Buffer& valueBuffer = model.buffers[valueBufferView.buffer];
                const float* values = reinterpret_cast<const float*>(&valueBuffer.data[valueBufferView.byteOffset + valueAccessor.byteOffset]);

                for (size_t k = 0; k < timeAccessor.count; ++k) {
                    float time = times[k];

                    JointKeyframe* kf = nullptr;
                    auto kfIt = std::find_if(jointAnim.keyframes.begin(), jointAnim.keyframes.end(), [&](const JointKeyframe& item) {
                        return std::abs(item.time - time) < 1e-4f;
                    });

                    if (kfIt != jointAnim.keyframes.end()) {
                        kf = &(*kfIt);
                    } else {
                        JointKeyframe newKf;
                        newKf.time = time;
                        newKf.translation = outJoints[0].translation;
                        newKf.rotation = outJoints[0].rotation;
                        newKf.rotationQuat = outJoints[0].rotationQuat;
                        newKf.isQuaternion = true;
                        newKf.scale = outJoints[0].scale;
                        jointAnim.keyframes.push_back(newKf);
                        kf = &jointAnim.keyframes.back();
                    }

                    if (channel.target_path == "translation") {
                        kf->translation.x = values[k * 3 + 0];
                        kf->translation.y = values[k * 3 + 1];
                        kf->translation.z = values[k * 3 + 2] * -1.0f;
                    } else if (channel.target_path == "rotation") {
                        kf->rotationQuat.x = values[k * 4 + 0] * -1.0f;
                        kf->rotationQuat.y = values[k * 4 + 1] * -1.0f;
                        kf->rotationQuat.z = values[k * 4 + 2];
                        kf->rotationQuat.w = values[k * 4 + 3];
                        kf->rotation = Math::ToEuler(kf->rotationQuat);
                        kf->isQuaternion = true;
                    } else if (channel.target_path == "scale") {
                        kf->scale.x = values[k * 3 + 0];
                        kf->scale.y = values[k * 3 + 1];
                        kf->scale.z = values[k * 3 + 2];
                    }
                }

                std::sort(jointAnim.keyframes.begin(), jointAnim.keyframes.end(), [](const JointKeyframe& a, const JointKeyframe& b) {
                    return a.time < b.time;
                });
            }

            if (!jointAnim.keyframes.empty()) {
                outMotions.push_back(motionData);
            }
        }
    }

    return true;
}


