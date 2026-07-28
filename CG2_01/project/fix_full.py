import os
filepath = 'Engine/Graphics/3D/SkinnedModel.cpp'
with open(filepath, 'r', encoding='utf-8') as f:
    lines = f.readlines()

# find void SkinnedModel::AddCubeMesh
start_idx = -1
for i, line in enumerate(lines):
    if line.startswith('void SkinnedModel::AddCubeMesh'):
        start_idx = i
        break

# find void SkinnedModel::ApplyMotion(float time)
end_idx = -1
for i, line in enumerate(lines):
    if line.startswith('void SkinnedModel::ApplyMotion(float time)'):
        end_idx = i
        break

new_content = '''void SkinnedModel::AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex) {
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
        { { 0, 2, 3, 1 }, { 0.0f, 0.0f, -1.0f } }, // 前
        { { 1, 3, 7, 5 }, { 1.0f, 0.0f, 0.0f } },  // 右
        { { 5, 7, 6, 4 }, { 0.0f, 0.0f, 1.0f } },  // 後
        { { 4, 6, 2, 0 }, { -1.0f, 0.0f, 0.0f } }, // 左
        { { 2, 6, 7, 3 }, { 0.0f, 1.0f, 0.0f } },  // 上
        { { 4, 0, 1, 5 }, { 0.0f, -1.0f, 0.0f } }  // 下
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
    for (size_t i = 0; i < joints_.size(); ++i) {
        if (joints_[i].isQuaternion) {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotationQuat, joints_[i].translation);
        } else {
            joints_[i].localMatrix = Math::MakeAffineMatrix(joints_[i].scale, joints_[i].rotation, joints_[i].translation);
        }

        if (joints_[i].parentIndex == -1) {
            joints_[i].globalMatrix = joints_[i].localMatrix;
        } else {
            int parentIdx = joints_[i].parentIndex;
            joints_[i].globalMatrix = Math::Multiply(joints_[i].localMatrix, joints_[parentIdx].globalMatrix);
        }
    }
    
    // Update jointBuffer_
    if (jointBuffer_ && !joints_.empty()) {
        Matrix4x4* mappedMatrices = nullptr;
        if (SUCCEEDED(jointBuffer_->Map(0, nullptr, (void**)&mappedMatrices))) {
            for (size_t i = 0; i < joints_.size(); ++i) {
                mappedMatrices[i] = Math::Multiply(joints_[i].offsetMatrix, joints_[i].globalMatrix);
            }
            jointBuffer_->Unmap(0, nullptr);
        }
    }
}

void SkinnedModel::CreateBuffers(DirectXCommon* dxCommon) {
    if (skinnedVertices_.empty()) return;

    auto device = dxCommon->GetDevice();
    UINT sizeVB = static_cast<UINT>(sizeof(SkinnedVertexData) * skinnedVertices_.size());

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
        SkinnedVertexData* vertMap = nullptr;
        vertexBuffer_->Map(0, nullptr, (void**)&vertMap);
        std::copy(skinnedVertices_.begin(), skinnedVertices_.end(), vertMap);
        vertexBuffer_->Unmap(0, nullptr);

        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeVB;
        vertexBufferView_.StrideInBytes = sizeof(SkinnedVertexData);
    }
    
    // Create jointBuffer_
    if (!joints_.empty()) {
        UINT sizeJoints = static_cast<UINT>(sizeof(Matrix4x4) * joints_.size());
        resDesc.Width = sizeJoints;
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&jointBuffer_));
    }
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

'''

with open(filepath, 'w', encoding='utf-8') as f:
    f.writelines(lines[:start_idx])
    f.write(new_content)
    f.writelines(lines[end_idx:])
print("Done")
