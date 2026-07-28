#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "MyMath.h"

class StageMapGimmickSystem;

// ブロック種類
enum class BlockType : uint32_t {
    None = 0,
    Ground,
    Wall,
    Ladder,
    Star,
    BubblePickup,
    Goal,
    PlayerStart,
    Door,
    PSwitch,
    PBlock,
    CrumblingFloor,
    IceBlock,
    MovingFloor,
    Key,            // 拾える鍵
    KeyBlock,       // 鍵で開くブロック
    Spike,          // トゲ
    EnemyWalker,    // 敵（歩行）
    EnemyFlyer,     // 敵（飛行）
    EnemyChaser,    // 敵（追尾）
    PBlockAppears,  // 押すと出現するPブロック
	Checkpoint,     // 中間地点
	TimedBlock,     // 時間差ブロック
    OnOffSwitch,    // ON/OFF切り替えスイッチ
    OnBlock,        // ONの時に実体化するブロック（赤）
    OffBlock,       // OFFの時に実体化するブロック（青）
    TransparentBlock
};

struct MovingFloorRef {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct EnemyRef {
    int x = 0;
    int y = 0;
    int z = 0;
};

inline const char* BlockTypeToString(BlockType type) {
    switch (type) {
    case BlockType::None:              return "None";
    case BlockType::Ground:            return "Ground";
    case BlockType::Wall:              return "Wall";
    case BlockType::Ladder:            return "Ladder";
    case BlockType::Star:              return "Star";
    case BlockType::BubblePickup:      return "BubblePickup";
    case BlockType::Goal:              return "Goal";
    case BlockType::PlayerStart:       return "PlayerStart";
    case BlockType::Door:              return "Door";
    case BlockType::PSwitch:           return "PSwitch";
    case BlockType::PBlock:            return "PBlock";
    case BlockType::CrumblingFloor:    return "CrumblingFloor";
    case BlockType::IceBlock:          return "IceBlock";
    case BlockType::MovingFloor:       return "MovingFloor";
    case BlockType::Key:               return "Key";
    case BlockType::KeyBlock:          return "KeyBlock";
    case BlockType::Spike:             return "Spike";
    case BlockType::EnemyWalker:       return "EnemyWalker";
    case BlockType::EnemyFlyer:        return "EnemyFlyer";
    case BlockType::EnemyChaser:       return "EnemyChaser";
    case BlockType::PBlockAppears:     return "PBlock (On)";
	case BlockType::Checkpoint:        return "Checkpoint";
	case BlockType::TimedBlock:        return "TimedBlock";
    case BlockType::OnOffSwitch:       return "OnOffSwitch";
    case BlockType::OnBlock:           return "OnBlock";
    case BlockType::OffBlock:          return "OffBlock";
    case BlockType::TransparentBlock:  return "TransparentBrock";
    default:                           return "Unknown";
    }
}

// シャボン玉の中身のエンコード・デコード用ヘルパー関数
inline int PackBubbleContents(BlockType type, int customId) {
    return (static_cast<int>(type) & 0xFFFF) | ((customId & 0xFFFF) << 16);
}
inline BlockType UnpackBubbleType(int packed) {
    return static_cast<BlockType>(packed & 0xFFFF);
}
inline int UnpackBubbleCustomId(int packed) {
    return (packed >> 16) & 0xFFFF;
}

// 3次元整数座標
struct Int3 {
    int x;
    int y;
    int z;
};



// 1マス分のデータ
struct MapCell {
    BlockType type = BlockType::None;
    int variant = 0;      // 見た目違い用。今は使わなくてOK
    bool isSolid = false; // 当たり判定用
	float rotationX = 0.0f; // X軸回転（オブジェクトの向きを変えたい）
	float rotationY = 0.0f; // Y軸回転（オブジェクトの向きを変えたい）
    Int3 doorTargetIndex = { 0,0,0 };

    // 崩れる足場用のタイマー管理
    float crumbleTimer = 0.0f;
    bool isCrumbling = false; // プレイヤーが乗っているフラグ
    // --- 復活ギミック用に追加 ---
    bool isHidden = false;      // 現在消えているかどうか
    float respawnTimer = 0.0f;  // 復活までのカウント
    // --- カラー演出用 ---
    float colorR = 1.0f; // 赤
    float colorG = 1.0f; // 緑 (1.0で通常、0.0に近づくと赤くなる)
    float colorB = 1.0f; // 青
    float opacity = 1.0f; // 透明度 (1.0で表示、0.0で非表示)

    // ▼ 追加：動く足場用（どの方向に何マス動くか）
    Int3 moveOffset{ 0, 0, 0 };
    // 動く足場の計算用データ
    float moveTimer = 0.0f;                  // サイン波計算用のタイマー
    // 現在の滑らかな移動オフセット
    float currentOffsetX = 0.0f;
    float currentOffsetY = 0.0f;
    float currentOffsetZ = 0.0f;

    // 1フレームあたりの移動量（差分：プレイヤーを一緒に引っ張るために使用）
    float deltaOffsetX = 0.0f;
    float deltaOffsetY = 0.0f;
    float deltaOffsetZ = 0.0f;
};

struct CustomBlockCell {
    BlockType type = BlockType::None;
};

// カスタムブロックパーツのプロパティ定義 (3x3x3 の複合ブロックアセンブリ)
struct CustomBlockPart {
    int id = 0;              // 1〜5 がカスタムパーツスロット
    std::string name = "";   // パーツ名
    BlockType baseType = BlockType::Wall; // 互換性用のベース種類
    float colorR = 1.0f;     // カスタムカラー
    float colorG = 1.0f;
    float colorB = 1.0f;

    CustomBlockCell cells[3][3][3]; // [y][z][x] 3Dアセンブリ形状データ

    bool IsEmpty() const {
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    if (cells[y][z][x].type != BlockType::None) return false;
                }
            }
        }
        return true;
    }
};

class StageMap {
    friend class StageMapGimmickSystem;
public:
    StageMap() = default;
    ~StageMap() = default;

    // サイズ指定で初期化
    void Initialize(int width, int height, int depth);

    // 追加
    void Update(float deltaTime, const Vector3& playerPos);

    // 時間差ブロック用の経過時間操作メソッド
    void ResetTime() { accumulatedTime_ = 0.0f; }
    float GetAccumulatedTime() const { return accumulatedTime_; }

    // ステージデータをファイルに保存する
    void SaveToFile(const std::string& filename);
    // ファイルからステージデータを読み込む
    void LoadFromFile(const std::string& filename);

    // 全消し
    void Clear();

    // 範囲内か
    bool IsInside(int x, int y, int z) const;
    bool IsInside(const Int3& index) const;

    // 取得
    const MapCell* GetCell(int x, int y, int z) const;
    const MapCell* GetCell(const Int3& index) const;

    MapCell* GetCell(int x, int y, int z);
    MapCell* GetCell(const Int3& index);

    // 設置
    bool SetBlock(int x, int y, int z, BlockType type, int variant = 0);
    bool SetBlock(const Int3& index, BlockType type, int variant = 0);

    // 削除
    bool RemoveBlock(int x, int y, int z);
    bool RemoveBlock(const Int3& index);

    // サイズ取得
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    int GetDepth() const { return depth_; }

    void SetPSwitchActive(int switchId);
    void ResetPSwitchStateNoRebuild();


	// 再構築が必要かどうかのフラグを返す関数
    bool NeedsRebuild() const { return needsRebuild_; }

    // フラグを「再構築の必要なし（false）」に戻す
    void ResetRebuildFlag() {
        needsRebuild_ = false;
    }

	// フラグを「再構築の必要なし（false）」に戻す
    void ClearRebuildFlag() { needsRebuild_ = false; }

	// フラグを「再構築の必要あり（true）」にセットする関数
    void RequestRebuild() { needsRebuild_ = true; }

	// Pスイッチが現在アクティブかどうかを返す関数
    bool IsPSwitchActive() const { return isPSwitchActive_; }

	// Pスイッチの状態をリセットする関数（再構築も要求する）
    void ResetPSwitchState();

    // --- カスタムブロックパーツ関連 ---
    const std::vector<CustomBlockPart>& GetCustomParts() const { return customParts_; }
    std::vector<CustomBlockPart>& GetCustomParts() { return customParts_; }
    const CustomBlockPart* GetCustomPart(int id) const {
        if (id >= 1 && id <= (int)customParts_.size()) {
            return &customParts_[id - 1];
        }
        return nullptr;
    }

	// Mutable 版の GetCustomPart
    CustomBlockPart* GetCustomPart(int id) {
        if (id >= 1 && id <= (int)customParts_.size()) {
            return &customParts_[id - 1];
        }
        return nullptr;
    }

    // ImGui描画用
    void DrawImGui();

    // 動く足場用のワールド座標当たり判定
    const MapCell* GetIntersectingMovingFloor(float pX, float pY, float pZ, float rX, float rY, float rZ) const;

    // ▼ 追加：指定座標から繋がっている鍵ブロックをすべて消去する関数
    void RemoveConnectedKeyBlocks(int x, int y, int z);

	// ★ 追加：動く足場のリストを再構築する関数（ロード後やサイズ変更後に呼ぶ）
    void RebuildMovingFloorList();
    // ★ 追加：敵キャラクターのリストを再構築する関数
    void RebuildEnemyList();
    const std::vector<EnemyRef>& GetEnemies() const { return enemies_; }

    // 環境設定（背景色、ライト）のゲッター・セッター
    const Vector4& GetClearColor() const { return clearColor_; }
    void SetClearColor(const Vector4& color) { clearColor_ = color; }

	// ライトの強さ、色、方向のゲッター・セッター
    float GetLightIntensity() const { return lightIntensity_; }
    void SetLightIntensity(float intensity) { lightIntensity_ = intensity; }

	// ライトの色のゲッター・セッター
    const Vector3& GetLightColor() const { return lightColor_; }
    void SetLightColor(const Vector3& color) { lightColor_ = color; }

	// ライトの方向のゲッター・セッター
    const Vector3& GetLightDirection() const { return lightDirection_; }
    void SetLightDirection(const Vector3& dir) { lightDirection_ = dir; }

	// 天候プリセット名のゲッター・セッター
    const std::string& GetWeatherPresetName() const { return weatherPresetName_; }
    void SetWeatherPresetName(const std::string& name) { weatherPresetName_ = name; }




    /// <summary>
    /// 指定した座標のドアと同じID（variant）を持つ、相方のドアの座標を検索する
    /// </summary>
    Int3 FindPairedDoor(int srcX, int srcY, int srcZ) const;

    // 🌟 状態取得・変更関数を追加
    bool IsOnState() const { return isOnState_; }

    void ToggleOnState();

private:

    int width_ = 0;
    int height_ = 0;
    int depth_ = 0;

    std::vector<MapCell> cells_;
    std::vector<CustomBlockPart> customParts_; // カスタムブロックパーツ定義リスト (スロット1〜5)
    std::vector<MovingFloorRef> movingFloors_;
    std::vector<EnemyRef> enemies_;

    // 環境・ライティング設定の保存値 (初期値)
    std::string weatherPresetName_ = "Sunny (Default)";
    Vector4 clearColor_ = { 0.1f, 0.25f, 0.5f, 1.0f }; // デフォルトの青背景
    float lightIntensity_ = 1.0f;
    Vector3 lightColor_ = { 0.9f, 0.9f, 0.9f };
    Vector3 lightDirection_ = { 0.5f, -1.0f, 0.5f };


private:

    int ToIndex(int x, int y, int z) const;
    MapCell MakeCell(BlockType type, int variant);

    bool isPSwitchActive_ = false; // Pスイッチの状態
    bool needsRebuild_ = false; // ★追加
    float accumulatedTime_ = 0.0f; // 累積時間を保存する変数

    bool isOnState_ = true; // 🌟 初期状態はON
};
