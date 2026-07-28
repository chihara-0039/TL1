#pragma once

#include <string>
#include <vector>
#include "ParticleManager.h"

// パーティクル演出プリセットをJSONへ保存・読み込みするためのデータアクセス層。
class EffectPresetStore {
public:
    // プリセット一覧取得の結果。showcaseはデモ再生対象として使う。
    struct PresetNames {
        std::vector<std::string> all;
        std::vector<std::string> showcase;
        std::string status;
    };

    // ヒット/斬撃系エフェクトの保存単位。
    struct HitPreset {
        ParticleManager::HitEffectSettings settings{};
        bool showGpuSphere = true;
        bool mirrorSlash = false;
        bool includeInShowcase = true;
        int burstCount = 1;
        float burstRadius = 0.0f;
    };

    // 雨・嵐など継続発生するエフェクトの保存単位。
    struct StormPreset {
        ParticleManager::StormEffectSettings settings{};
        bool includeInShowcase = true;
    };

    // 指定ファイルからヒット系プリセット名一覧を読み込む。
    PresetNames LoadHitPresetNames(const std::string& path) const;
    // 指定ファイルからストーム系プリセット名一覧を読み込む。defaultNameは一覧が空のときの基準名。
    PresetNames LoadStormPresetNames(const std::string& path, const std::string& defaultName) const;

    // ヒット系プリセットを保存・読み込みする。statusにはUI表示用の結果文を入れる。
    bool SaveHitPreset(const std::string& path, const std::string& name, const HitPreset& preset, std::string& status) const;
    bool LoadHitPreset(const std::string& path, const std::string& name, HitPreset& preset, std::string& status) const;

    // ストーム系プリセットを保存・読み込みする。defaultNameは旧データ互換のフォールバック名。
    bool SaveStormPreset(const std::string& path, const std::string& name, const StormPreset& preset, std::string& status) const;
    bool LoadStormPreset(const std::string& path, const std::string& defaultName, const std::string& name, StormPreset& preset, std::string& status) const;
};
