#include "StageMap.h"
#include "StageMapGimmickSystem.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <Windows.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void StageMap::Initialize(int width, int height, int depth) {
    // ステージの3次元グリッドサイズを決め、全セルを確保する。
    assert(width > 0);
    assert(height > 0);
    assert(depth > 0);

    width_ = width;
    height_ = height;
    depth_ = depth;

    cells_.resize(width_ * height_ * depth_);

    // ステージ読み込み時に、前ステージのギミック状態が残らないようにリセットする。
    isPSwitchActive_ = false;
    isOnState_ = true;
    needsRebuild_ = false;
    accumulatedTime_ = 0.0f;


    Clear();

    // 5つのカスタムブロックパーツスロットをデフォルト値で初期化
    customParts_.resize(5);
    for (int i = 0; i < 5; ++i) {
        customParts_[i].id = i + 1;
        customParts_[i].baseType = BlockType::Wall;
        customParts_[i].colorR = 1.0f;
        customParts_[i].colorG = 1.0f;
        customParts_[i].colorB = 1.0f;

        // 全セルを None に初期化
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    customParts_[i].cells[y][z][x].type = BlockType::None;
                }
            }
        }
    }

    // エディタで最初から使えるカスタムブロックのプリセット形状を用意する。
    // Slot 1: L-SHIELD (L字の壁足場パーツ)
    customParts_[0].name = "L-SHIELD";
    customParts_[0].colorR = 0.9f; customParts_[0].colorG = 0.3f; customParts_[0].colorB = 0.3f; // スタイリッシュ赤
    customParts_[0].cells[0][0][0].type = BlockType::Wall;
    customParts_[0].cells[0][0][1].type = BlockType::Wall;
    customParts_[0].cells[0][0][2].type = BlockType::Wall;
    customParts_[0].cells[1][0][2].type = BlockType::Wall;
    customParts_[0].cells[2][0][2].type = BlockType::Wall;

    // Slot 2: T-BRIDGE (T字足場パーツ)
    customParts_[1].name = "T-BRIDGE";
    customParts_[1].colorR = 0.9f; customParts_[1].colorG = 0.8f; customParts_[1].colorB = 0.2f; // ゴールド黄色
    customParts_[1].cells[0][0][1].type = BlockType::Wall;
    customParts_[1].cells[1][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][0].type = BlockType::Wall;
    customParts_[1].cells[2][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][2].type = BlockType::Wall;

    // Slot 3: LADDER-WALL (ハシゴ付き壁)
    customParts_[2].name = "LADDER-WALL";
    customParts_[2].colorR = 0.2f; customParts_[2].colorG = 0.7f; customParts_[2].colorB = 0.9f; // ライトブルー
    customParts_[2].cells[0][0][1].type = BlockType::Wall;
    customParts_[2].cells[1][0][1].type = BlockType::Wall;
    customParts_[2].cells[2][0][1].type = BlockType::Wall;
    customParts_[2].cells[0][0][0].type = BlockType::Ladder;
    customParts_[2].cells[1][0][0].type = BlockType::Ladder;
    customParts_[2].cells[2][0][0].type = BlockType::Ladder;

    // Slot 4 & 5: 空白のカスタム用スロット
    customParts_[3].name = "MY PART A";
    customParts_[3].colorR = 0.4f; customParts_[3].colorG = 0.9f; customParts_[3].colorB = 0.4f; // ライムグリーン
    customParts_[3].cells[0][0][0].type = BlockType::Wall; // 1マスだけ

    customParts_[4].name = "MY PART B";
    customParts_[4].colorR = 0.8f; customParts_[4].colorG = 0.4f; customParts_[4].colorB = 0.9f; // パープル
    customParts_[4].cells[0][0][0].type = BlockType::Ladder; // 1マスだけ
}

void StageMap::Update(float deltaTime, const Vector3& playerPos)
{
    // ステージ全体で共有する経過時間。時間制ブロックなどの周期演出に使う。
    accumulatedTime_ += deltaTime;

    // 時間制ブロックは variant の十の位をグループ、 一の位を出現順として扱う。
    // まず各グループに存在する出現順を集め、ステージごとに必要な周期を求められるようにする。
    std::vector<int> groupOrders[10];
    for (const auto& cell : cells_) {
        if (cell.type == BlockType::TimedBlock) {
            int group = cell.variant / 10;
            int order = cell.variant % 10;
            if (group >= 1 && group < 10) {
                if (std::find(groupOrders[group].begin(), groupOrders[group].end(), order) == groupOrders[group].end()) {
                    groupOrders[group].push_back(order);
                }
            }
        }
    }

    // 出現順に並べることで、同じグループ内のブロックを順番に表示/非表示できる。
    for (int g = 1; g < 10; ++g) {
        if (!groupOrders[g].empty()) {
            std::sort(groupOrders[g].begin(), groupOrders[g].end());
        }
    }

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                MapCell& cell = cells_[ToIndex(x, y, z)];

                if (cell.type == BlockType::TimedBlock) {
                    // 同じグループ内で、order の小さいブロックから順に出現させる。
                    int group = cell.variant / 10;
                    int order = cell.variant % 10;
                    if (group >= 1 && group < 10 && !groupOrders[group].empty()) {
                        const auto& orders = groupOrders[group];

                        float kAppearDelay = 1.2f;       // 1.2秒間隔で次のブロックが出現
                        float kActiveDuration = 3.0f;    // 3.0秒間表示する
                        float kRestDuration = 1.5f;      // 全て消えた後のインターバル

                        // グループ内の最後のブロックが消えた後、少し待ってから最初に戻る。
                        float cycleDuration = static_cast<float>(orders.size() - 1) * kAppearDelay + kActiveDuration + kRestDuration;

                        // このブロックの出現順インデックスを取得
                        auto it = std::find(orders.begin(), orders.end(), order);
                        size_t idx = std::distance(orders.begin(), it);

						// 出現時間と消滅時間を計算
                        float appearTime = static_cast<float>(idx) * kAppearDelay;
                        float disappearTime = appearTime + kActiveDuration;

						// 現在の経過時間を周期内に収める
                        float localCycleTime = std::fmod(accumulatedTime_, cycleDuration);
                        if (localCycleTime >= appearTime && localCycleTime < disappearTime) {
                            cell.isSolid = true;
                        } else {
                            cell.isSolid = false;
                        }
                    }
                }

#pragma region 崩れる足場

                if (cell.type == BlockType::CrumblingFloor) {
                    // --- 崩れる処理 ---
                    if (!cell.isHidden) {
                        if (cell.crumbleTimer > 0.0f || cell.isCrumbling) {
                            cell.crumbleTimer += deltaTime;
                            // ここが崩れるタイマー
                            if (cell.crumbleTimer >= 1.0f) {
                                cell.isHidden = true;
                                cell.isSolid = false;
                            }
                        }
                    }

                    // --- 復活処理 ---
                    if (cell.isHidden) {
                        cell.respawnTimer += deltaTime;
                        if (cell.respawnTimer >= 3.0f) { // 3秒で復活
                            cell.isHidden = false;
                            cell.isSolid = true; // 判定復活
                            cell.respawnTimer = 0.0f;
                            cell.crumbleTimer = 0.0f;
                        }
                    }

                    // --- 演出用の色・透明度計算 ---
                    if (!cell.isHidden) {
                        // crumbleTimerが0なら白、1.0に近づくほど赤くなる
                        float r = cell.crumbleTimer / 1.0f;
                        cell.colorG = 1.0f - r;
                        cell.colorB = 1.0f - r;
                        cell.opacity = 1.0f - r; // ★追加：乗っている間は徐々に透明（フェードアウト）にする
                    }
                    else {
                        cell.opacity = 0.0f;     // ★追加：完全に消えている間は透明度 0
                    }
                    cell.isCrumbling = false;
                }
#pragma endregion

                if (cell.type == BlockType::MovingFloor) {
                    // MovingFloor は moveOffset で指定された目的地まで往復する。
                    // currentOffset は描画位置、deltaOffset は乗っているプレイヤーを一緒に動かすために使う。
                    float moveSpeed = 1.0f;
                    cell.moveTimer += deltaTime * moveSpeed;

                    float moveRate = (std::sin(cell.moveTimer) + 1.0f) / 2.0f;

                    // 前フレームのオフセットを記憶
                    float oldX = cell.currentOffsetX;
                    float oldY = cell.currentOffsetY;
                    float oldZ = cell.currentOffsetZ;

                    // 新しいオフセットを計算
                    cell.currentOffsetX = static_cast<float>(cell.moveOffset.x) * moveRate;
                    cell.currentOffsetY = static_cast<float>(cell.moveOffset.y) * moveRate;
                    cell.currentOffsetZ = static_cast<float>(cell.moveOffset.z) * moveRate;

                    // 1フレーム分の移動量を記録
                    cell.deltaOffsetX = cell.currentOffsetX - oldX;
                    cell.deltaOffsetY = cell.currentOffsetY - oldY;
                    cell.deltaOffsetZ = cell.currentOffsetZ - oldZ;
                }

                // --- 敵1 (EnemyWalker): 左右往復 (X軸) ---
                if (cell.type == BlockType::EnemyWalker) {
                    cell.moveTimer += deltaTime * 2.0f;
                    cell.currentOffsetX = std::sin(cell.moveTimer) * 2.0f;
                    cell.currentOffsetY = 0.0f;
                    cell.currentOffsetZ = 0.0f;
                }

                // --- 敵2 (EnemyFlyer): 上下往復 (Y軸) ---
                if (cell.type == BlockType::EnemyFlyer) {
                    cell.moveTimer += deltaTime * 1.5f;
                    cell.currentOffsetY = std::sin(cell.moveTimer) * 2.5f;
                    cell.currentOffsetX = 0.0f;
                    cell.currentOffsetZ = 0.0f;
                }

                // --- 敵3 (EnemyChaser): プレイヤー追尾 (一定範囲内のみ) ---
                if (cell.type == BlockType::EnemyChaser) {
                    // スポーン地点から一定範囲内にプレイヤーが入ると追尾し、遠ざかると初期位置へ戻る。
                    Vector3 spawnPos = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
                    Vector3 currentPos = {
                        spawnPos.x + cell.currentOffsetX,
                        spawnPos.y + cell.currentOffsetY,
                        spawnPos.z + cell.currentOffsetZ
                    };
                    Vector3 toPlayer = {
                        playerPos.x - currentPos.x,
                        playerPos.y - currentPos.y,
                        playerPos.z - currentPos.z
                    };
                    float distanceToPlayer = std::sqrt(
                        toPlayer.x * toPlayer.x +
                        toPlayer.y * toPlayer.y +
                        toPlayer.z * toPlayer.z);

					// プレイヤーがスポーン位置からどれくらい離れているかを計算
                    float playerDistanceFromSpawn = std::sqrt(
                        (playerPos.x - spawnPos.x) * (playerPos.x - spawnPos.x) +
                        (playerPos.y - spawnPos.y) * (playerPos.y - spawnPos.y) +
                        (playerPos.z - spawnPos.z) * (playerPos.z - spawnPos.z)
                    );

					// プレイヤーがスポーン位置から8マス以内にいる場合のみ追尾する
                    if (playerDistanceFromSpawn < 8.0f && distanceToPlayer > 0.05f) {
                        float speed = 1.2f; // 秒速 1.2 マス
                        Vector3 directionToPlayer = {
                            toPlayer.x / distanceToPlayer,
                            toPlayer.y / distanceToPlayer,
                            toPlayer.z / distanceToPlayer
                        };
                        Vector3 nextPos = {
                            currentPos.x + directionToPlayer.x * (speed * deltaTime),
                            currentPos.y + directionToPlayer.y * (speed * deltaTime),
                            currentPos.z + directionToPlayer.z * (speed * deltaTime)
                        };

                        // スポーン位置からの最大追尾距離を 8 マスに制限
                        Vector3 nextOffset = {
                            nextPos.x - spawnPos.x,
                            nextPos.y - spawnPos.y,
                            nextPos.z - spawnPos.z
                        };
						// 8 マス以上離れないように制限する
                        float offsetDistanceFromSpawn = std::sqrt(
                            nextOffset.x * nextOffset.x +
                            nextOffset.y * nextOffset.y +
                            nextOffset.z * nextOffset.z);
                        if (offsetDistanceFromSpawn > 8.0f) {
                            nextOffset = {
                                nextOffset.x / offsetDistanceFromSpawn * 8.0f,
                                nextOffset.y / offsetDistanceFromSpawn * 8.0f,
                                nextOffset.z / offsetDistanceFromSpawn * 8.0f
                            };
                        }
                        cell.currentOffsetX = nextOffset.x;
                        cell.currentOffsetY = nextOffset.y;
                        cell.currentOffsetZ = nextOffset.z;
                    }
                    else {
                        // プレイヤーが遠い場合は、ゆっくり初期位置に戻る
                        float returnSpeed = 1.0f;
                        float currentOffsetDistance = std::sqrt(
                            cell.currentOffsetX * cell.currentOffsetX +
                            cell.currentOffsetY * cell.currentOffsetY +
                            cell.currentOffsetZ * cell.currentOffsetZ);
                        if (currentOffsetDistance > 0.05f) {
                            cell.currentOffsetX -= (cell.currentOffsetX / currentOffsetDistance) * returnSpeed * deltaTime;
                            cell.currentOffsetY -= (cell.currentOffsetY / currentOffsetDistance) * returnSpeed * deltaTime;
                            cell.currentOffsetZ -= (cell.currentOffsetZ / currentOffsetDistance) * returnSpeed * deltaTime;
                        }
                        else {
                            cell.currentOffsetX = 0.0f;
                            cell.currentOffsetY = 0.0f;
                            cell.currentOffsetZ = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

// ステージデータをファイルに保存する。SaveToFile はステージサイズを最初に書き出すため、LoadFromFile が最初にこの3値を読んでグリッドを確保する。
void StageMap::SaveToFile(const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return;

    // 1行目は必ずステージサイズにする。LoadFromFile が最初にこの3値を読んでグリッドを確保する。
    ofs << width_ << " " << height_ << " " << depth_ << "\n";

    // ステージごとの見た目を再現するため、天候プリセットとライト設定も保存する。
    ofs << "PRESET \"" << weatherPresetName_ << "\"\n";
    ofs << "ENVIRONMENT "
        << clearColor_.x << " " << clearColor_.y << " " << clearColor_.z << " " << clearColor_.w << " "
        << lightIntensity_ << " "
        << lightColor_.x << " " << lightColor_.y << " " << lightColor_.z << " "
        << lightDirection_.x << " " << lightDirection_.y << " " << lightDirection_.z << "\n";



    // カスタムブロック定義を書き出す。
    // PART はスロット全体の見た目、PARTCELL は 3x3x3 内の実セル構成を表す。
    for (const auto& part : customParts_) {
        ofs << "PART " << part.id << " "
            << static_cast<int>(part.baseType) << " "
            << part.colorR << " "
            << part.colorG << " "
            << part.colorB << " "
            << part.name << "\n";

        // アセンブリの各セルで None でないものを書き出す
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    if (part.cells[y][z][x].type != BlockType::None) {
                        ofs << "PARTCELL " << part.id << " "
                            << x << " " << y << " " << z << " "
                            << static_cast<int>(part.cells[y][z][x].type) << "\n";
                    }
                }
            }
        }
    }

    // 通常ブロックの配置情報を書き出す。空セルは保存せず、ファイルサイズを小さく保つ。
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell->type == BlockType::None) continue; // 空ブロックは保存しない

                int saveVariant = cell->variant;
                if (cell->type == BlockType::MovingFloor) {
                    // MovingFloor は移動先オフセットを variant にパックして保存する。
                    // +10 しているのは負のオフセットも 0 以上の値として扱うため。
                    saveVariant = (cell->moveOffset.x + 10) | ((cell->moveOffset.y + 10) << 8) | ((cell->moveOffset.z + 10) << 16);
                }

				// 座標、ブロック種別、回転角度、variant を保存する。
                ofs << x << " " << y << " " << z << " "
                    << static_cast<int>(cell->type) << " "
                    << cell->rotationX << " " << cell->rotationY << " "
                    << saveVariant << "\n";

				// ドアブロックの場合、ドア先座標を保存する。
                if (cell->type == BlockType::Door) {
                    ofs << cell->doorTargetIndex.x << " "
                        << cell->doorTargetIndex.y << " "
                        << cell->doorTargetIndex.z << "\n"; // ドア先座標の後に改行を出力して安全に読み込めるようにする
                }
            }
        }
    }
    ofs.close();
}

// ステージファイルを読み込む。LoadFromFile はステージサイズを最初に読み込むため、Initialize() でグリッドを確保する。
void StageMap::LoadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    // 最初の行からステージサイズを復元し、そのサイズでセル配列を作り直す。
    std::string firstLine;
    if (!std::getline(ifs, firstLine)) return;

	// 1行目の "width height depth" を読み込む
    std::stringstream ss(firstLine);
    int w, h, d;
    if (!(ss >> w >> h >> d)) return;
    Initialize(w, h, d);

    // 環境設定をデフォルト値に初期化（ファイルに記述がない場合用）
    clearColor_ = { 0.1f, 0.25f, 0.5f, 1.0f };
    lightIntensity_ = 1.0f;
    lightColor_ = { 0.9f, 0.9f, 0.9f };
    lightDirection_ = { 0.5f, -1.0f, 0.5f };

    // PARTCELL があるスロットだけプリセット形状を消して、ファイル側の定義で上書きする。
    bool partCleared[5] = { false, false, false, false, false };

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;

		// 行の先頭トークンを取得して、行の種類を判定する。
        std::stringstream lineSS(line);
        std::string token;
        lineSS >> token;

        if (token == "PART") {
            // カスタムブロックのスロット情報。色や名前など、UI と描画に使う設定を復元する。
            int id, baseTypeVal;
            float r, g, b;
            lineSS >> id >> baseTypeVal >> r >> g >> b;
            std::string name;
            std::getline(lineSS, name);
            // 先頭のスペースを除去
            if (!name.empty() && name[0] == ' ') {
                name = name.substr(1);
            }

			// パーツスロットの範囲内であれば復元する
            if (id >= 1 && id <= (int)customParts_.size()) {
                auto& part = customParts_[id - 1];
                part.id = id;
                part.baseType = static_cast<BlockType>(baseTypeVal);
                part.colorR = r;
                part.colorG = g;
                part.colorB = b;
                part.name = name;
            }
        } else if (token == "PARTCELL") {
            // カスタムブロックの内部セル情報。3x3x3 のどこに何のブロックがあるかを復元する。
            int id, lx, ly, lz, typeVal;
			// 互換性重視のパース設計：
            if (lineSS >> id >> lx >> ly >> lz >> typeVal) {
                // 座標が 0～2 の範囲内であることを確認してから配置する
                if (id >= 1 && id <= (int)customParts_.size()) {
                    auto& part = customParts_[id - 1];

                    // ファイルにアセンブリセル情報があるスロットのみ、初回出現時に元のプリセット形状をクリアして適用
                    if (!partCleared[id - 1]) {
                        for (int y = 0; y < 3; ++y) {
                            for (int z = 0; z < 3; ++z) {
                                for (int x = 0; x < 3; ++x) {
                                    part.cells[y][z][x].type = BlockType::None;
                                }
                            }
                        }
                        partCleared[id - 1] = true;
                    }

					// 座標が 0～2 の範囲内であることを確認してから配置する
                    if (lx >= 0 && lx < 3 && ly >= 0 && ly < 3 && lz >= 0 && lz < 3) {
                        part.cells[ly][lz][lx].type = static_cast<BlockType>(typeVal);
                    }
                }
            }
			// ステージの見た目を再現するため、天候プリセット名とライト設定も復元する。
        } else if (token == "PRESET") {
            // 天候プリセット名は空白を含められるようにダブルクォート付きで保存している。
            std::string presetName;
            std::getline(lineSS, presetName);
            // " " を取り除く
            size_t start = presetName.find('\"');
            size_t end = presetName.rfind('\"');
            if (start != std::string::npos && end != std::string::npos && start < end) {
                weatherPresetName_ = presetName.substr(start + 1, end - start - 1);
            }
		// 天候プリセット名を復元したら、ステージの見た目を再現するために ApplyWeatherPreset を呼ぶ。
        } else if (token == "ENVIRONMENT") {
            // 背景色、ライト強度、ライト色、ライト方向を復元する。
            lineSS >> clearColor_.x >> clearColor_.y >> clearColor_.z >> clearColor_.w
                   >> lightIntensity_
                   >> lightColor_.x >> lightColor_.y >> lightColor_.z
                   >> lightDirection_.x >> lightDirection_.y >> lightDirection_.z;
        } else {
            // 通常のブロック配置行（token は x 座標）
            int x = std::stoi(token);
            int y, z, typeVal;
            float rotX, rotY;
            int variant = 0;

            // 互換性重視の完璧なパース設計：
            // 残りパラメータが 5個（y z typeVal rotX rotY）以上あればパース成功
            if (lineSS >> y >> z >> typeVal >> rotX >> rotY) {
                // さらに variant があれば読み込む（なければデフォルトの0を使用）
                lineSS >> variant;

				// 座標がステージ内にあるか確認してから配置する
                BlockType type = static_cast<BlockType>(typeVal);
                SetBlock(x, y, z, type, variant);
                MapCell* cell = GetCell(x, y, z);
				// 角度を復元する
                if (cell) {
                    cell->rotationX = rotX;
                    cell->rotationY = rotY;

                    // ドアの追加データ
                    if (cell->type == BlockType::Door) {
                        ifs >> cell->doorTargetIndex.x
                            >> cell->doorTargetIndex.y
                            >> cell->doorTargetIndex.z;
                        // 改行を消費
                        std::string dummy;
                        std::getline(ifs, dummy);
                    }
                }
            }
        }
    }
    ifs.close();
    RebuildMovingFloorList();
    RebuildEnemyList();
}

// ステージ全体を初期化する（全セルを None に置き換える）
void StageMap::Clear() {
    for (MapCell& cell : cells_) {
        cell = MapCell{};
        cell.type = BlockType::None;
        cell.variant = 0;
        cell.isSolid = false;
    }

    movingFloors_.clear();
    enemies_.clear();

    isPSwitchActive_ = false;
    isOnState_ = true;
    needsRebuild_ = false;

    ResetTime();
}

// 指定された座標がステージ内にあるかどうかを判定する
bool StageMap::IsInside(int x, int y, int z) const {
    return
        x >= 0 && x < width_ &&
        y >= 0 && y < height_ &&
        z >= 0 && z < depth_;
}

// Int3 版の IsInside
bool StageMap::IsInside(const Int3& index) const {
    return IsInside(index.x, index.y, index.z);
}

// セルのインデックスを 1 次元配列に変換する
const MapCell* StageMap::GetCell(int x, int y, int z) const {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

// Int3 版の GetCell
const MapCell* StageMap::GetCell(const Int3& index) const {
    return GetCell(index.x, index.y, index.z);
}

// Mutable 版の GetCell
MapCell* StageMap::GetCell(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

// Int3 版の GetCell
MapCell* StageMap::GetCell(const Int3& index) {
    return GetCell(index.x, index.y, index.z);
}

// 指定座標のブロックを新しいタイプとバリアントで置き換える
bool StageMap::SetBlock(int x, int y, int z, BlockType type, int variant) {
    if (!IsInside(x, y, z)) {
        return false;
    }

	// 指定座標のブロックを新しいタイプとバリアントで置き換える
    cells_[ToIndex(x, y, z)] = MakeCell(type, variant);
    RebuildMovingFloorList();
    RebuildEnemyList();
    return true;
}

// Int3 版の SetBlock
bool StageMap::SetBlock(const Int3& index, BlockType type, int variant) {
    return SetBlock(index.x, index.y, index.z, type, variant);
}

// 指定座標のブロックを削除する（None に置き換える）
bool StageMap::RemoveBlock(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return false;
    }

	// 指定座標のブロックを None に置き換える
    cells_[ToIndex(x, y, z)] = MakeCell(BlockType::None, 0);
    RebuildMovingFloorList();
    RebuildEnemyList();

    return true;
}

// Int3 版の RemoveBlock
bool StageMap::RemoveBlock(const Int3& index) {
    return RemoveBlock(index.x, index.y, index.z);
}

// PSwitch の状態を切り替える
void StageMap::SetPSwitchActive(int switchId) {
    StageMapGimmickSystem::SetPSwitchActive(*this, switchId);
}

// PSwitch の状態をリセットする（再構築なし）
void StageMap::ResetPSwitchStateNoRebuild() {
    StageMapGimmickSystem::ResetPSwitchStateNoRebuild(*this);
}

// ステージ全体の ON/OFF 状態を切り替える
void StageMap::ToggleOnState() {
    StageMapGimmickSystem::ToggleOnState(*this);
}

void StageMap::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Size: %d x %d x %d", width_, height_, depth_);

    // 固定位置のセルの情報など（デバッグ用）
    const MapCell* cell = GetCell(2, 1, 0);
    if (cell) {
        ImGui::Text("Cell(2,1,0) type = %d", static_cast<int>(cell->type));
        ImGui::Text("Cell(2,1,0) solid = %s", cell->isSolid ? "true" : "false");
    }
#endif
}

// ★ 追加：動く足場とのワールド座標（AABBボックス型）当たり判定の実装
const MapCell* StageMap::GetIntersectingMovingFloor(
    float pX, float pY, float pZ,
    float rX, float rY, float rZ
) const {
    for (const auto& ref : movingFloors_) {
        const MapCell* cell = GetCell(ref.x, ref.y, ref.z);
        if (!cell || cell->type != BlockType::MovingFloor) {
            continue;
        }

		// 動く足場の中心座標を計算（セルの中心 + 現在のオフセット）
        float floorCenterX = static_cast<float>(ref.x) + cell->currentOffsetX;
        float floorCenterY = static_cast<float>(ref.y) + 0.5f + cell->currentOffsetY;
        float floorCenterZ = static_cast<float>(ref.z) + cell->currentOffsetZ;

		// プレイヤーの中心座標を計算（AABBの中心）
        float playerCenterX = pX;
        float playerCenterY = pY + rY;
        float playerCenterZ = pZ;

        float blockSize = 0.5f;

		// AABB（軸平行境界ボックス）同士の交差判定
        if (std::abs(playerCenterX - floorCenterX) < (rX + blockSize) &&
            std::abs(playerCenterY - floorCenterY) < (rY + blockSize) &&
            std::abs(playerCenterZ - floorCenterZ) < (rZ + blockSize)) {
            return cell;
        }
    }

    return nullptr;
}

void StageMap::RemoveConnectedKeyBlocks(int x, int y, int z)
{
    // マップの範囲外なら処理を抜ける
    if (x < 0 || x >= width_ || y < 0 || y >= height_ || z < 0 || z >= depth_) {
        return;
    }

    // 指定座標のセルを取得
    MapCell* cell = GetCell(x, y, z);

    // セルが存在しない、または「鍵ブロック」でなければ処理を抜ける
    // (既に None になっている場合もここで止まるため、無限ループを防げます)
    if (!cell || cell->type != BlockType::KeyBlock) {
        return;
    }

    // 自身のブロックを消去する
    cell->type = BlockType::None;
    cell->isSolid = false;

    // 上下左右前後の6方向に対して、同じ処理を芋づる式に呼び出す（再帰呼び出し）
    RemoveConnectedKeyBlocks(x + 1, y, z); // 右
    RemoveConnectedKeyBlocks(x - 1, y, z); // 左
    RemoveConnectedKeyBlocks(x, y + 1, z); // 上
    RemoveConnectedKeyBlocks(x, y - 1, z); // 下
    RemoveConnectedKeyBlocks(x, y, z + 1); // 前
    RemoveConnectedKeyBlocks(x, y, z - 1); // 後
}

Int3 StageMap::FindPairedDoor(int srcX, int srcY, int srcZ) const
{
    const MapCell* srcCell = GetCell(srcX, srcY, srcZ);
    // そもそも指定座標がドアではない、または存在しない場合は元の座標を返す
    if (!srcCell || srcCell->type != BlockType::Door) {
        return { srcX, srcY, srcZ };
    }

    // 入ったドアの番号（ID）
    int targetDoorId = srcCell->variant;

    // マップ全体をループして、同じドア番号を持つ「別の座標のドア」を探す
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                // 自分自身の座標はスキップ
                if (x == srcX && y == srcY && z == srcZ) continue;

                const MapCell* cell = GetCell(x, y, z);
                // 同じ種類のブロック（Door）かつ、同じ識別番号（variant）のセルが見つかった場合
                if (cell && cell->type == BlockType::Door && cell->variant == targetDoorId) {
                    return { x, y, z }; // 相方の座標を返す
                }
            }
        }
    }

    // 万が一、相方のドアが見つからなかった（1つしか配置していない）場合はワープさせず元の座標を返す
    return { srcX, srcY, srcZ };
}

// 座標 (x, y, z) を 1 次元配列のインデックスに変換する
int StageMap::ToIndex(int x, int y, int z) const {
    return x + (z * width_) + (y * width_ * depth_);
}

// Int3 版の ToIndex
MapCell StageMap::MakeCell(BlockType type, int variant) {
    MapCell cell{};
    cell.type = type;
    cell.variant = variant;

    switch (type) {
    case BlockType::None:
    cell.isSolid = false;
    break;

    case BlockType::Ground:
    case BlockType::Wall:
    case BlockType::Star:
    case BlockType::CrumblingFloor:
    case BlockType::IceBlock:
    case BlockType::KeyBlock:    // 鍵ブロックは通り抜けられない
    case BlockType::PBlock:
    case BlockType::TransparentBlock:
    cell.isSolid = true;
    break;

    case BlockType::MovingFloor:
    cell.isSolid = true;
    if (variant == 0) {
        cell.moveOffset = { 0, 0, 3 }; // デフォルト Z 軸に 3 マス
    } else {
        int dx = (variant & 0xFF) - 10;
        int dy = ((variant >> 8) & 0xFF) - 10;
        int dz = ((variant >> 16) & 0xFF) - 10;
        if (dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10 && dz >= -10 && dz <= 10) {
            cell.moveOffset = { dx, dy, dz };
        } else {
            cell.moveOffset = { 0, 0, 3 };
        }
    }
    break;

    case BlockType::BubblePickup:
    case BlockType::Goal:
    case BlockType::PlayerStart:
    case BlockType::Door:
    case BlockType::PSwitch:
    case BlockType::Key:         // 鍵は通り抜けられる
    case BlockType::Checkpoint:  // 🌟 追加：中間地点は通り抜けられる
    case BlockType::TimedBlock:  // 🌟 追加：時間差ブロック（最初は消えている）
    cell.isSolid = false;
    break;

    case BlockType::OnBlock:
        // ON状態なら実体化（true）、OFF状態ならすり抜ける（false）
        cell.isSolid = isOnState_; 
        break;

    case BlockType::OffBlock:
        // OFF状態なら実体化（true）、ON状態ならすり抜ける（false）
        cell.isSolid = !isOnState_; 
        break;
        
    case BlockType::OnOffSwitch:
        // スイッチ自体は叩く必要があるので常に当たり判定を持たせる
        cell.isSolid = false;
        break;
    }

    return cell;
}

// PSwitch の状態をリセットする（再構築あり）
void StageMap::ResetPSwitchState()
{
    StageMapGimmickSystem::ResetPSwitchState(*this);
}

// 動く足場のリストを再構築する
void StageMap::RebuildMovingFloorList() {
    movingFloors_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell && cell->type == BlockType::MovingFloor) {
                    movingFloors_.push_back({ x, y, z });
                }
            }
        }
    }
}

// 敵のリストを再構築する
void StageMap::RebuildEnemyList() {
    enemies_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell && (cell->type == BlockType::EnemyWalker || 
                             cell->type == BlockType::EnemyFlyer || 
                             cell->type == BlockType::EnemyChaser)) {
                    enemies_.push_back({ x, y, z });
                }
            }
        }
    }
}
