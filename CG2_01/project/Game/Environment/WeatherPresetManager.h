#pragma once

#include "MyMath.h"
#include <string>
#include <vector>

// 背景色、ライト、天候パーティクルをまとめた環境プリセット。
struct WeatherPreset {
    std::string name;

    // 環境色とライト設定。
    Vector4 clearColor = {0.1f, 0.25f, 0.5f, 1.0f};
    float lightIntensity = 1.0f;
    Vector3 lightColor = {1.0f, 1.0f, 1.0f};
    Vector3 lightDirection = {0.5f, -1.0f, 0.5f};

    // 天球の色倍率と明るさ。
    Vector4 skyColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float skyBrightness = 1.0f;

    // 背景用パーティクル雲。高度はステージ最上面からの余白で指定する。
    bool cloudEnabled = true;
    Vector4 cloudColor = {0.72f, 0.78f, 0.90f, 0.22f};
    float cloudDensity = 0.45f;
    float cloudSize = 1.4f;
    float cloudAltitudeOffset = 6.0f;

    // 雨や雪などの天候パーティクル設定。
    bool particleEnabled = false;
    std::string particleTexture = "Resources/UI/inventory/white.png";
    float emitRate = 100.0f;
    Vector3 emitSize = {40.0f, 20.0f, 40.0f};
    Vector3 velocity = {0.0f, -5.0f, 0.0f};
    Vector3 velocityRandom = {1.0f, 0.5f, 1.0f};
    Vector3 particleSize = {0.2f, 0.2f, 0.2f};
    float particleLife = 4.0f;
    Vector4 particleColor = {1.0f, 1.0f, 1.0f, 1.0f};
    std::string impactEffect = "None"; ///< None / Rain / Snow
    std::string stormPreset; ///< 空でなければ継続エフェクトプリセットを天候として再生する。
};

// 天候プリセットの読み込み、保存、検索を担当するシングルトン。
class WeatherPresetManager {
public:
    static WeatherPresetManager& GetInstance() {
        static WeatherPresetManager instance;
        return instance;
    }

    // JSONファイルからプリセット一覧を読み込む。
    void LoadPresets();
    // 現在のプリセット一覧をJSONファイルへ保存する。
    void SavePresets();

    const std::vector<WeatherPreset>& GetPresets() const { return presets_; }
    std::vector<WeatherPreset>& GetPresets() { return presets_; }
    
    // プリセットを名前で検索
    WeatherPreset* GetPresetByName(const std::string& name);

    // デフォルトプリセットをいくつか追加する
    void CreateDefaultPresetsIfEmpty();

private:
    WeatherPresetManager() = default;
    ~WeatherPresetManager() = default;
    
    std::vector<WeatherPreset> presets_;
    const std::string filepath_ = "Resources/presets/weather_presets.json";
};
