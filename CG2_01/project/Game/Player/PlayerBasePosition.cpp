#include "PlayerBasePosition.h"
#include "Player.h"

bool PlayerBasePosition::ApplyFromStageMap(const StageMap& stageMap, Player* player) {
    if (!player) {
        return false;
    }

    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {

                const MapCell* cell = stageMap.GetCell(x, y, z);

                if (cell && cell->type == BlockType::PlayerStart) {
                    index_ = { x, y, z };

                    position_ = {
                        static_cast<float>(x),
                        static_cast<float>(y) + 1.1f,
                        static_cast<float>(z)
                    };

                    player->SetPosition(position_);
                    player->SetRespawnPosition(position_);

                    return true;
                }
            }
        }
    }

    return false;
}