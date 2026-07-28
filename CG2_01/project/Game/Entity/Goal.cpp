#include "Goal.h"
#include<cmath>

bool Goal::Check(const Vector3& pos, const Vector3& radius, const StageMap& map) {
    float checkOffsetsY[] = { 0.1f, 0.8f, 1.5f };

    for (float dy : checkOffsetsY) {
        for (float dx : { -radius.x, radius.x }) {
            for (float dz : { -radius.z, radius.z }) {

                int gx = static_cast<int>(std::floor(pos.x + dx + 0.5f));
                int gy = static_cast<int>(std::floor(pos.y + dy));
                int gz = static_cast<int>(std::floor(pos.z + dz + 0.5f));

                const MapCell* cell = map.GetCell(gx, gy, gz);

                if (cell && cell->type == BlockType::Goal) {
                    return true;
                }
            }
        }
    }
    return false;
}