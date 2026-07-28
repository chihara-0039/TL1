#pragma once

class Input;
class ParticleManager;
class PostProcessRenderer;

/// CG5評価課題用のポストエフェクト閲覧モードを管理する。
/// キー割り当てと説明UIを同じクラスに置き、表示と操作の不一致を防ぐ。
class PostEffectShowcaseController {
public:
    /// シーン遷移時に、再生途中の演出も含めて通常描画へ戻す。
    void Reset(PostProcessRenderer& postProcess);

    /// エフェクト切り替えを処理する。TABが押された場合はtrueを返す。
    bool Update(Input& input, ParticleManager* particleManager, PostProcessRenderer& postProcess);

    /// 現在のエフェクト名とRelease用操作ガイドを描画する。
    void DrawImGui(const PostProcessRenderer& postProcess) const;

    /// 通常ゲーム内で数字キーによるPostEffect切り替えを処理する。
    /// Showcaseと違い、天候やゲームパーティクルは停止しない。
    void UpdateGameplay(Input& input, PostProcessRenderer& postProcess);

    /// 通常ゲーム内へ現在の効果と評価用キーガイドを表示する。
    void DrawGameplayImGui(const PostProcessRenderer& postProcess) const;

private:
    void StartDissolve(PostProcessRenderer& postProcess);
    void UpdateDissolve(PostProcessRenderer& postProcess);
    void ReturnToNormal(PostProcessRenderer& postProcess);

    bool dissolvePlaying_ = false;
    bool returnToNormalNextFrame_ = false;
    float dissolveThreshold_ = 0.0f;
    static constexpr float kDissolveDurationSeconds = 2.0f;
};
