import os
filepath = 'Engine/Graphics/3D/SkinnedObject.cpp'
with open(filepath, 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    if 'object3dCommon->PreDrawPlayerHighlight();' in line:
        pass

# Actually let's just find the object3dCommon->PreDrawPlayerHighlight(); and or (size_t i = 0; i < numJoints; ++i) { and inject the missing lines
start_idx = -1
for i, line in enumerate(lines):
    if 'object3dCommon->PreDrawPlayerHighlight();' in line:
        start_idx = i
        break

if start_idx != -1:
    end_idx = -1
    for i in range(start_idx, len(lines)):
        if 'if (parentIdx == -1) continue;' in lines[i]:
            end_idx = i
            break
    
    if end_idx != -1:
        replacement = '''    object3dCommon->PreDrawPlayerHighlight();

    // 関節 (ジョイント) の描画
    for (size_t i = 0; i < numJoints; ++i) {
        // ジョイントのグローバル行列に、オブジェクト自体のワールド行列を掛け合わせる
        Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);

        // カメラ設定
        jointVisuals_[i]->SetCamera(view, projection);
        
        Vector3 globalPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };
        jointVisuals_[i]->SetPosition(globalPos);
        jointVisuals_[i]->SetRotation({ 0, 0, 0 });
        jointVisuals_[i]->SetScale({ 0.04f, 0.04f, 0.04f });

        if (static_cast<int>(i) == selectedJointIndex_) {
            jointVisuals_[i]->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
        } else {
            jointVisuals_[i]->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
        }

        jointVisuals_[i]->SetEnableLighting(false);
        jointVisuals_[i]->Update(Math::MakeIdentity4x4());
        jointVisuals_[i]->Draw();
    }

    size_t boneVisualCount = 0;
    for (size_t i = 0; i < numJoints; ++i) {
        int parentIdx = joints[i].parentIndex;
        if (parentIdx == -1) continue;
'''
        
        del lines[start_idx:end_idx+1]
        lines.insert(start_idx, replacement)

with open(filepath, 'w', encoding='utf-8') as f:
    f.writelines(lines)
