import re

with open("Engine/Graphics/3D/SkinnedModel.cpp", "r", encoding="utf-8") as f:
    content = f.read()

# Replace SmoothWeights
start_idx = content.find("void SkinnedModel::SmoothWeights()")
end_idx = content.find("void SkinnedModel::Update(DirectXCommon* dxCommon)")

new_smooth_weights = """void SkinnedModel::SmoothWeights() {
    for (auto& v : skinnedVertices_) {
        Vector3 pos = { v.position.x, v.position.y, v.position.z };
        int primaryJoint = v.jointIndices[0];

        // 1. Spine (1) と Pelvis (0) の境界付近の補間
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
        
        // 2. 腕関節
        if (primaryJoint == 3 && pos.x < -0.45f) {
            float dist = (pos.x - (-0.45f)) / (-0.1f);
            float weightElbow = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 3; v.weights[0] = 1.0f - weightElbow;
            v.jointIndices[1] = 4; v.weights[1] = weightElbow;
        } else if (primaryJoint == 4 && pos.x > -0.55f) {
            float dist = (pos.x - (-0.55f)) / 0.1f;
            float weightShoulder = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 4; v.weights[0] = 1.0f - weightShoulder;
            v.jointIndices[1] = 3; v.weights[1] = weightShoulder;
        }
        
        if (primaryJoint == 6 && pos.x > 0.45f) {
            float dist = (pos.x - 0.45f) / 0.1f;
            float weightElbow = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 6; v.weights[0] = 1.0f - weightElbow;
            v.jointIndices[1] = 7; v.weights[1] = weightElbow;
        } else if (primaryJoint == 7 && pos.x < 0.55f) {
            float dist = (pos.x - 0.55f) / (-0.1f);
            float weightShoulder = std::max(0.0f, std::min(1.0f, dist));
            v.jointIndices[0] = 7; v.weights[0] = 1.0f - weightShoulder;
            v.jointIndices[1] = 6; v.weights[1] = weightShoulder;
        }
    }
}
"""

content = content[:start_idx] + new_smooth_weights + content[end_idx:]

with open("Engine/Graphics/3D/SkinnedModel.cpp", "w", encoding="utf-8") as f:
    f.write(content)
