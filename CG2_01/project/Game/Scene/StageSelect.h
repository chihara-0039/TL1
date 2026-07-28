#pragma once
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include <vector>
#include <string>
#include <memory>

// ステージ選択画面の入力、選択中ステージ表示、決定状態を管理する。
class StageSelect
{
public:
	// ステージ一覧とプレビュー表示用オブジェクトを初期化する。
	void Initialize(Object3dCommon* objCommon, Input* input, int startIndex = 0);

	// 入力に応じて選択ステージとプレビュー回転を更新する。
	void Update();

	// 選択中ステージのプレビューを描画する。
	void Draw();

	bool IsFnished() const { return isFinished_; }
	int GetSelectedIndex() const { return selectedStageIndex_; }
	// 決定済みステージのファイル名を返す。呼び出し側はIsFnished後の使用を想定する。
	std::string GetSelectedFileName() const { return stageFiles_[selectedStageIndex_]; }
private:
	Object3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	Camera camera_;

	// 選択可能なステージファイル名の一覧。
	std::vector<std::string> stageFiles_;

	int selectedStageIndex_ = 0;
	bool isFinished_ = false;

	// ステージ選択画面で回転表示するプレビュー用モデル。
	std::unique_ptr<Model> stageModel_;
	std::unique_ptr<Object3d> stageObject_;

	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

	float targetRotationX_ = 0.0f;
	float targetRotationY_ = 0.0f;
	float currentRotationX_ = 0.0f;
	float currentRotationY_ = 0.0f;
};
