#include "SkinningEditorController.h"
#include "Player.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "ParticleManager.h"
#include "externals/imgui/imgui.h"
#include "json.hpp"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>   // sprintf_s
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>

namespace {
	using json = nlohmann::json;

	const char* GetModelAssetKind(int index, int objStartIndex, int gltfStartIndex) {
		if (index == 0) {
			return "SKIN";
		}
		if (index >= objStartIndex && index < gltfStartIndex) {
			return "OBJ";
		}
		return "GLTF";
	}

	const char* GetModelAssetDisplayKind(int index, int objStartIndex, int gltfStartIndex, bool hasThumbnail) {
		if (index >= objStartIndex && index < gltfStartIndex && hasThumbnail) {
			return "OBJ+IMG";
		}
		if (index >= gltfStartIndex && hasThumbnail) {
			return "GLTF+IMG";
		}

		return GetModelAssetKind(index, objStartIndex, gltfStartIndex);
	}

	ImVec4 GetModelAssetColor(int index, int objStartIndex, int gltfStartIndex) {
		if (index == 0) {
			return ImVec4(0.32f, 0.50f, 0.76f, 1.0f);
		}
		if (index >= objStartIndex && index < gltfStartIndex) {
			return ImVec4(0.42f, 0.63f, 0.78f, 1.0f);
		}
		return ImVec4(0.35f, 0.62f, 0.52f, 1.0f);
	}

	std::string GetDisplayFileName(const std::string& path, const std::string& fallback) {
		if (path == "Default") {
			return fallback;
		}

		std::filesystem::path filePath(path);
		std::string fileName = filePath.filename().string();
		return fileName.empty() ? fallback : fileName;
	}

	bool IsObjAssetIndex(int index, int objStartIndex, int gltfStartIndex) {
		return index >= objStartIndex && index < gltfStartIndex;
	}

	bool SplitModelPath(const std::string& fullPath, std::string& directory, std::string& fileName) {
		if (fullPath.empty() || fullPath == "Default") {
			return false;
		}

		std::filesystem::path path(fullPath);
		directory = path.parent_path().generic_string();
		fileName = path.filename().generic_string();
		return !directory.empty() && !fileName.empty();
	}

	std::string ResolveLevelModelPath(const std::string& fileName) {
		if (fileName.empty()) {
			return "";
		}

		std::filesystem::path path(fileName);
		if (path.has_parent_path() && std::filesystem::exists(path)) {
			return path.generic_string();
		}
		if (path.has_parent_path()) {
			return path.generic_string();
		}

		const std::string stem = path.stem().string();
		std::vector<std::filesystem::path> candidates;
		candidates.push_back(std::filesystem::path("Resources/Models") / stem / (stem + ".obj"));
		candidates.push_back(std::filesystem::path("Resources/Models") / (stem + ".obj"));
		candidates.push_back(std::filesystem::path("Resources/Models") / stem / fileName);

		for (const auto& candidate : candidates) {
			if (std::filesystem::exists(candidate)) {
				return candidate.generic_string();
			}
		}

		return fileName;
	}

	json Vector3ToJson(const Vector3& value) {
		return json::array({ value.x, value.y, value.z });
	}

	/// <summary>OBJ頂点からモデル空間の中心と全体サイズを求める。</summary>
	bool ReadObjLocalBounds(const std::string& filePath, Vector3& outCenter, Vector3& outSize) {
		std::ifstream file(filePath);
		if (!file.is_open()) {
			return false;
		}

		const float highest = (std::numeric_limits<float>::max)();
		Vector3 minimum{ highest, highest, highest };
		Vector3 maximum{ -highest, -highest, -highest };
		bool foundVertex = false;
		std::string line;
		while (std::getline(file, line)) {
			if (line.rfind("v ", 0) != 0) {
				continue;
			}
			std::istringstream stream(line.substr(2));
			Vector3 vertex{};
			if (!(stream >> vertex.x >> vertex.y >> vertex.z)) {
				continue;
			}
			minimum.x = (std::min)(minimum.x, vertex.x);
			minimum.y = (std::min)(minimum.y, vertex.y);
			minimum.z = (std::min)(minimum.z, vertex.z);
			maximum.x = (std::max)(maximum.x, vertex.x);
			maximum.y = (std::max)(maximum.y, vertex.y);
			maximum.z = (std::max)(maximum.z, vertex.z);
			foundVertex = true;
		}
		if (!foundVertex) {
			return false;
		}

		outCenter = {
			(minimum.x + maximum.x) * 0.5f,
			(minimum.y + maximum.y) * 0.5f,
			(minimum.z + maximum.z) * 0.5f };
		outSize = {
			maximum.x - minimum.x,
			maximum.y - minimum.y,
			maximum.z - minimum.z };
		return true;
	}

	Vector3 JsonToVector3(const json& value, const Vector3& fallback) {
		if (!value.is_array() || value.size() < 3) {
			return fallback;
		}

		return {
			value.at(0).get<float>(),
			value.at(1).get<float>(),
			value.at(2).get<float>()
		};
	}

	bool DrawTexturedModelTile(ImTextureID textureId, const ImVec2& size, bool selected, const char* kind) {
		const ImVec2 start = ImGui::GetCursorScreenPos();
		const bool clicked = ImGui::InvisibleButton("textured_model_tile", size);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		const ImU32 bgColor = selected
			? IM_COL32(214, 156, 58, 255)
			: IM_COL32(58, 65, 76, 255);
		const ImU32 sideColor = IM_COL32(44, 50, 60, 255);
		const ImU32 lineColor = IM_COL32(218, 226, 238, 230);
		const ImU32 labelBg = IM_COL32(21, 25, 31, 210);
		const ImU32 labelText = IM_COL32(245, 248, 252, 255);

		const ImVec2 end = ImVec2(start.x + size.x, start.y + size.y);
		drawList->AddRectFilled(start, end, bgColor, 4.0f);

		const float frontW = size.x * 0.58f;
		const float frontH = size.y * 0.46f;
		const float depth = size.x * 0.16f;
		const float cx = start.x + size.x * 0.47f;
		const float cy = start.y + size.y * 0.50f;

		ImVec2 f0 = ImVec2(cx - frontW * 0.5f, cy - frontH * 0.5f);
		ImVec2 f1 = ImVec2(cx + frontW * 0.5f, cy - frontH * 0.5f);
		ImVec2 f2 = ImVec2(cx + frontW * 0.5f, cy + frontH * 0.5f);
		ImVec2 f3 = ImVec2(cx - frontW * 0.5f, cy + frontH * 0.5f);
		ImVec2 b0 = ImVec2(f0.x + depth, f0.y - depth * 0.55f);
		ImVec2 b1 = ImVec2(f1.x + depth, f1.y - depth * 0.55f);
		ImVec2 b2 = ImVec2(f2.x + depth, f2.y - depth * 0.55f);

		drawList->AddQuadFilled(b0, b1, f1, f0, IM_COL32(75, 84, 96, 255));
		drawList->AddQuadFilled(f1, b1, b2, f2, sideColor);
		drawList->AddImageQuad(textureId, f0, f1, f2, f3);
		drawList->AddQuad(f0, f1, f2, f3, lineColor, 1.5f);
		drawList->AddLine(f0, b0, lineColor, 1.2f);
		drawList->AddLine(f1, b1, lineColor, 1.2f);
		drawList->AddLine(f2, b2, lineColor, 1.2f);
		drawList->AddLine(b0, b1, lineColor, 1.2f);
		drawList->AddLine(b1, b2, lineColor, 1.2f);

		ImVec2 labelMin = ImVec2(start.x + 5.0f, start.y + 5.0f);
		ImVec2 labelMax = ImVec2(start.x + size.x - 5.0f, labelMin.y + 18.0f);
		drawList->AddRectFilled(labelMin, labelMax, labelBg, 3.0f);
		drawList->AddText(ImVec2(labelMin.x + 5.0f, labelMin.y + 2.0f), labelText, kind);

		return clicked;
	}

	std::string FindSidecarThumbnailPath(const std::string& modelPath) {
		if (modelPath == "Default") {
			return "";
		}

		const std::filesystem::path path(modelPath);
		const std::filesystem::path directory = path.parent_path();
		const std::filesystem::path stem = path.stem();
		const char* extensions[] = { ".png", ".jpg", ".jpeg" };

		for (const char* extension : extensions) {
			std::filesystem::path thumbnailPath = directory / (stem.string() + extension);
			if (std::filesystem::exists(thumbnailPath)) {
				std::string result = thumbnailPath.string();
				std::replace(result.begin(), result.end(), '\\', '/');
				return result;
			}
		}

		return "";
	}

} // namespace

// ==========================================================
//  SkinningEditorController::Initialize
//  SkinnedObject・デバッグキューブ・グリッド線・モデルリストを生成する
// ==========================================================
void SkinningEditorController::Initialize(
	Object3dCommon* object3dCommon,
	DirectXCommon* dxCommon,
	TextureManager* textureManager) {
	// 依存ポインタを保存 (所有権は持たない)
	object3dCommon_ = object3dCommon;
	dxCommon_ = dxCommon;
	textureManager_ = textureManager;

	// ----------------------------------------------------------
	// 1. glTF モデルのスキャン (Resources/Models 以下を再帰探索)
	// ----------------------------------------------------------
	ScanGltfModels();

	// ----------------------------------------------------------
	// 2. デバッグ用立方体モデルの読み込み (スケルトン描画に使用)
	// ----------------------------------------------------------
	debugCubeModel_ = std::unique_ptr<Model>(
		Model::CreateFromOBJ(dxCommon, "Resources/Models/cube", "cube.obj", textureManager));

	// ----------------------------------------------------------
	// 3. プレビュー用 SkinnedObject の生成と初期化
	//    起動時はインデックス 0 (デフォルト人型) で初期化する
	// ----------------------------------------------------------
	skinnedObject_ = std::make_unique<SkinnedObject>();
	ChangePreviewModel(0);
	skinnedObject_->SetPosition({ 0.0f, 0.0f, 0.0f }); // 地面 (Y=0) に接地
	skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });

	// ----------------------------------------------------------
	// 4. デバッグ用グリッド線の生成 (-10m 〜 +10m / 1m 刻み / X軸・Z軸方向)
	// ----------------------------------------------------------
	for (int i = -10; i <= 10; ++i) {
		// === X 方向に並ぶ縦線 (Z 方向に伸びる) ===
		auto lineX = std::make_unique<Object3d>();
		lineX->Initialize(object3dCommon);
		lineX->SetModel(debugCubeModel_.get());
		lineX->SetPosition({ (float)i, 0.0f, 0.0f });
		lineX->SetScale({ 0.015f, 0.002f, 10.0f }); // 極細・薄い
		lineX->SetRotation({ 0.0f, 0.0f, 0.0f });
		lineX->SetEnableLighting(false);
		// 中央 (X=0) は赤 (X軸色)、その他はグレー
		lineX->SetColor((i == 0)
			? Vector4{ 0.8f, 0.2f, 0.2f, 1.0f }
		: Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
		gridLines_.push_back(std::move(lineX));

		// === Z 方向に並ぶ横線 (X 方向に伸びる) ===
		auto lineZ = std::make_unique<Object3d>();
		lineZ->Initialize(object3dCommon);
		lineZ->SetModel(debugCubeModel_.get());
		lineZ->SetPosition({ 0.0f, 0.0f, (float)i });
		lineZ->SetScale({ 10.0f, 0.002f, 0.015f });
		lineZ->SetRotation({ 0.0f, 0.0f, 0.0f });
		lineZ->SetEnableLighting(false);
		// 中央 (Z=0) は青 (Z軸色)、その他はグレー
		lineZ->SetColor((i == 0)
			? Vector4{ 0.2f, 0.2f, 0.8f, 1.0f }
		: Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
		gridLines_.push_back(std::move(lineZ));
	}
}

// ==========================================================
//  SkinningEditorController::Update
//  レイキャスト選択・SkinnedObject 更新・グリッド線更新
// ==========================================================
void SkinningEditorController::Update(
	DirectXCommon* dxCommon,
	Input* input,
	Camera* camera,
	const Matrix4x4& lightVP,
	bool                 isGuiCaptured,
	ParticleManager* particleManager) {
	// ----------------------------------------------------------
	// 1. レイキャストによるジョイントクリック選択
	//    OBJ モードはスケルトンがないためこのブロックをスキップする
	//    (以前は関数全体を return していたため、カメラ更新も止まっていた → 修正済み)
	// ----------------------------------------------------------
	if (!isObjPreviewMode_ && skinnedObject_) {

		// ----------------------------------------------------------
		// 1. レイキャストによるジョイントクリック選択
		//    ImGui がマウスをキャプチャしている時はスキップする
		// ----------------------------------------------------------
		const auto& mouse = input->GetMouseState();
		static bool mouse0Pre = false;
		bool mouse0Trigger = mouse.buttons[0] && !mouse0Pre; // 左クリック立ち上がり
		mouse0Pre = mouse.buttons[0];

		if (mouse0Trigger && !isGuiCaptured) {
			// デバッグビルド時は左側 320px のオフセットがあるため補正する
			float mouseX = static_cast<float>(mouse.posX);
		#ifndef NDEBUG
			mouseX -= 320.0f;          // ビューポートの X オフセット (左パネル幅)
			float drawWidth = 1280.0f; // ビューポートの幅
		#else
			float drawWidth = static_cast<float>(WinApp::kClientWidth);
		#endif
			// NDC 座標に変換 (-1 〜 +1)
			float ndcX = (2.0f * mouseX) / drawWidth - 1.0f;
			float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.posY)) / WinApp::kClientHeight;

			// ビュー・プロジェクション行列の逆行列でレイをワールド空間に変換
			Matrix4x4 vp = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
			Matrix4x4 invVP = Math::Inverse(vp);

			// クリップ空間の Near / Far 点を逆投影するラムダ
			auto transformVec = [](const Vector4& v, const Matrix4x4& m) -> Vector4 {
				return {
					v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0],
					v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1],
					v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2],
					v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3],
				};
				};

			Vector4 nearW4 = transformVec({ ndcX, ndcY, 0.0f, 1.0f }, invVP);
			Vector4 farW4 = transformVec({ ndcX, ndcY, 1.0f, 1.0f }, invVP);

			Vector3 nearWorld = { nearW4.x / nearW4.w, nearW4.y / nearW4.w, nearW4.z / nearW4.w };
			Vector3 farWorld = { farW4.x / farW4.w,  farW4.y / farW4.w,  farW4.z / farW4.w };

			Vector3 rayOrigin = nearWorld;
			Vector3 rayDir = Math::Normalize(Math::Subtract(farWorld, nearWorld));

			// オブジェクトのワールド行列を使ってジョイントのワールド位置を計算
			const auto& joints = skinnedObject_->GetModel()->GetJoints();
			Matrix4x4 objWorld = Math::MakeAffineMatrix(
				skinnedObject_->GetScale(),
				skinnedObject_->GetRotation(),
				skinnedObject_->GetPosition());

			// 各ジョイントとレイの球交差判定 → 最も手前のジョイントを選択
			int   closestJointIndex = -1;
			float minT = FLT_MAX;
			const float clickRadius = 0.22f; // ボーンが選択しやすいよう少し大きめに設定

			for (size_t i = 0; i < joints.size(); ++i) {
				Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);
				Vector3   jointPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

				// レイと球の交差判定 (代数的手法)
				Vector3 m = Math::Subtract(rayOrigin, jointPos);
				float   b = m.x * rayDir.x + m.y * rayDir.y + m.z * rayDir.z;
				float   c = (m.x * m.x + m.y * m.y + m.z * m.z) - (clickRadius * clickRadius);

				if (c > 0.0f && b > 0.0f) { continue; } // 球の外側かつレイが逆方向 → スキップ

				float discr = b * b - c;
				if (discr < 0.0f) { continue; } // 判別式が負 → 交点なし

				float t = -b - std::sqrt(discr);
				if (t < 0.0f) { t = 0.0f; }

				if (t < minT) {
					minT = t;
					closestJointIndex = static_cast<int>(i);
				}
			}

			// 最も手前のジョイントを選択状態にする
			if (closestJointIndex != -1) {
				skinnedObject_->SetSelectedJointIndex(closestJointIndex);
			}
		}
	} // if (!isObjPreviewMode_ && skinnedObject_)

	// ----------------------------------------------------------
	// 2. モデルの更新 (OBJ / SkinnedObject を切り替える)
	//    OBJ モードでもここは必ず通る (レイキャストのみスキップした)
	// ----------------------------------------------------------
	if (isObjPreviewMode_ && objPreviewObject_) {
		// OBJ モード: 通常の Object3d を更新する
		objPreviewObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
		objPreviewObject_->Update(lightVP);
	} else if (skinnedObject_) {
		// glTF / デフォルト人型モード: SkinnedObject を更新する
		skinnedObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
		skinnedObject_->Update(dxCommon, lightVP);
	}

	UpdateHandParticleEmitter(particleManager);

	// Assets から配置した SceneObject は、UI で編集された Transform を毎フレーム Object3d に反映する。
	// 保存対象は SceneObject::transform 側なので、Object3d の値を直接編集せずここで同期する。
	// 選択中の配置物は Inspector でどれを編集しているか分かるように黄色寄りの色で強調する。
	for (int i = 0; i < static_cast<int>(sceneObjects_.size()); ++i) {
		SceneObject& sceneObject = sceneObjects_[i];
		if (sceneObject.disabled || !sceneObject.object) {
			continue;
		}

		sceneObject.object->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
		sceneObject.object->SetPosition(sceneObject.transform.translate);
		sceneObject.object->SetRotation(sceneObject.transform.rotate);
		sceneObject.object->SetScale(sceneObject.transform.scale);
		sceneObject.object->SetColor(i == selectedSceneObjectIndex_
			? Vector4{ 1.0f, 0.82f, 0.28f, 1.0f }
		: Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		sceneObject.object->Update(lightVP);
	}

	// ----------------------------------------------------------
	// 3. グリッド線の更新 (カメラ行列のセットと定数バッファ転送)
	//    SkinningEditor モード中のみ呼ばれるため、ここで安全に更新する
	// ----------------------------------------------------------
	for (auto& line : gridLines_) {
		line->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
		line->Update(lightVP);
	}
}

void SkinningEditorController::UpdateHandParticleEmitter(ParticleManager* particleManager) {
	if (!emitHandParticles_ || !particleManager || !skinnedObject_ || isObjPreviewMode_) {
		return;
	}

	// 初回だけ手に相当するジョイントを名前候補から探してキャッシュする。
	if (handParticleJointIndex_ < 0) {
		handParticleJointIndex_ = skinnedObject_->FindJointIndexByNameHints({
			"hand_r", "r_hand", "right_hand", "righthand", "hand.r",
			"hand_l", "l_hand", "left_hand", "lefthand", "hand.l", "hand"
		});
	}

	// アニメーション後のジョイント行列から、手の現在ワールド座標を取得する。
	Vector3 handPosition{};
	if (!skinnedObject_->TryGetJointWorldPosition(handParticleJointIndex_, handPosition)) {
		return;
	}

	// 毎フレーム出すと強すぎるため、一定間隔で小さな火花として発生させる。
	handParticleTimer_ += 1.0f / 60.0f;
	if (handParticleTimer_ < 0.18f) {
		return;
	}
	handParticleTimer_ = 0.0f;

	// 既存のヒットエフェクトを手元用に小さく調整して使う。
	ParticleManager::HitEffectSettings settings{};
	settings.size = 0.32f;
	settings.brightness = 0.85f;
	settings.lifeScale = 0.55f;
	settings.slashCount = 2;
	settings.sparkCount = 14;
	settings.sparkSpeed = 0.58f;
	settings.sparkLength = 0.45f;
	settings.scatterRadius = 0.22f;
	settings.ringPower = 0.15f;
	settings.corePower = 0.75f;
	settings.crossPower = 0.0f;
	settings.pillarPower = 0.0f;
	settings.lightningCount = 0;
	settings.randomizePosition = true;
	settings.randomizeDirection = true;
	settings.randomizeScale = true;
	settings.randomizeLifetime = true;
	settings.randomizeColor = true;
	settings.coreColor = { 1.0f, 0.52f, 0.14f, 1.0f };
	settings.slashColor = { 1.0f, 0.42f, 0.08f, 1.0f };
	settings.sparkColor = { 1.0f, 0.72f, 0.18f, 1.0f };
	settings.sparkSecondaryColor = { 0.34f, 0.72f, 1.0f, 1.0f };
	settings.ringColor = { 1.0f, 0.38f, 0.08f, 1.0f };

	particleManager->EmitHitEffect(handPosition, settings);
}

bool SkinningEditorController::PlaceSelectedAssetInScene() {
	return PlaceAssetInScene(selectedModelIndex_);
}

bool SkinningEditorController::PlaceAssetInScene(int assetIndex) {
	// SceneObject は Model / Object3d / TextureManager を使って生成するため、
	// 初期化前に呼ばれた場合は何も作らずステータスだけ返す。
	if (!object3dCommon_ || !dxCommon_ || !textureManager_) {
		sceneEditorStatus_ = "Scene placement failed: engine systems are not ready.";
		return false;
	}
	// 今回の保存可能シーン配置は、まず静的OBJに限定する。
	// glTF は SkinnedObject とアニメーション更新が絡むため、別の SceneObject 種別として拡張する想定。
	if (!IsObjAssetIndex(assetIndex, objStartIndex_, gltfStartIndex_)) {
		sceneEditorStatus_ = "Scene placement supports static OBJ assets first.";
		return false;
	}

	// Model::CreateFromOBJ は directory と filename を別々に受け取るため、
	// Assets が保持している相対パスをここで分解する。
	std::string directory;
	std::string fileName;
	if (!SplitModelPath(modelPaths_[assetIndex], directory, fileName)) {
		sceneEditorStatus_ = "Scene placement failed: invalid model path.";
		return false;
	}

	SceneObject sceneObject;
	sceneObject.name = GetDisplayFileName(modelPaths_[assetIndex], modelNames_[assetIndex]);
	sceneObject.assetPath = modelPaths_[assetIndex];
	// 連続配置したときに完全に重ならないよう、簡単なグリッド状の初期位置にずらして置く。
	// 位置は Inspector の Transform で後から調整できる。
	sceneObject.transform.translate = {
		static_cast<float>(sceneObjects_.size() % 5) * 1.5f - 3.0f,
		0.0f,
		static_cast<float>(sceneObjects_.size() / 5) * 1.5f
	};
	// Object3d は Model の所有権を持たないため、SceneObject 内で Model を保持して寿命を合わせる。
	sceneObject.model = Model::CreateFromOBJ(dxCommon_, directory, fileName, textureManager_);
	if (!sceneObject.model) {
		sceneEditorStatus_ = "Scene placement failed: model load failed.";
		return false;
	}

	sceneObject.object = std::make_unique<Object3d>();
	sceneObject.object->Initialize(object3dCommon_);
	sceneObject.object->SetModel(sceneObject.model.get());
	sceneObject.object->SetPosition(sceneObject.transform.translate);
	sceneObject.object->SetRotation(sceneObject.transform.rotate);
	sceneObject.object->SetScale(sceneObject.transform.scale);

	sceneObjects_.push_back(std::move(sceneObject));
	selectedSceneObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
	sceneEditorStatus_ = "Placed scene object: " + sceneObjects_.back().name;
	return true;
}

void SkinningEditorController::ClearSceneObjects() {
	sceneObjects_.clear();
	selectedSceneObjectIndex_ = -1;
	sceneEditorStatus_ = "Scene objects cleared.";
}

bool SkinningEditorController::ConsumePlayRequest() {
	const bool requested = playRequest_;
	playRequest_ = false;
	return requested;
}

std::vector<WorldCollisionBox> SkinningEditorController::BuildWorldCollisionBoxes() const {
	std::vector<WorldCollisionBox> boxes;
	for (const SceneObject& sceneObject : sceneObjects_) {
		if (sceneObject.disabled || !sceneObject.collider.enabled ||
			(!sceneObject.collider.type.empty() && sceneObject.collider.type != "BOX")) {
			continue;
		}

		// 回転したBOXもゲームの衝突処理で扱えるよう、8頂点を内包するAABBへ変換する。
		const Matrix4x4 world = Math::MakeAffineMatrix(
			sceneObject.transform.scale, sceneObject.transform.rotate, sceneObject.transform.translate);
		const Vector3 half = {
			std::abs(sceneObject.collider.size.x) * 0.5f,
			std::abs(sceneObject.collider.size.y) * 0.5f,
			std::abs(sceneObject.collider.size.z) * 0.5f };
		const float highest = (std::numeric_limits<float>::max)();
		WorldCollisionBox box;
		box.minimum = { highest, highest, highest };
		box.maximum = { -highest, -highest, -highest };

		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				for (int z = -1; z <= 1; z += 2) {
					const Vector3 local = {
						sceneObject.collider.center.x + half.x * static_cast<float>(x),
						sceneObject.collider.center.y + half.y * static_cast<float>(y),
						sceneObject.collider.center.z + half.z * static_cast<float>(z) };
					const Vector3 point = {
						local.x * world.m[0][0] + local.y * world.m[1][0] + local.z * world.m[2][0] + world.m[3][0],
						local.x * world.m[0][1] + local.y * world.m[1][1] + local.z * world.m[2][1] + world.m[3][1],
						local.x * world.m[0][2] + local.y * world.m[1][2] + local.z * world.m[2][2] + world.m[3][2] };
					box.minimum.x = (std::min)(box.minimum.x, point.x);
					box.minimum.y = (std::min)(box.minimum.y, point.y);
					box.minimum.z = (std::min)(box.minimum.z, point.z);
					box.maximum.x = (std::max)(box.maximum.x, point.x);
					box.maximum.y = (std::max)(box.maximum.y, point.y);
					box.maximum.z = (std::max)(box.maximum.z, point.z);
				}
			}
		}
		boxes.push_back(box);
	}

	return boxes;
}

bool SkinningEditorController::SaveSceneObjects(const std::string& filePath) {
	try {
		// 保存先フォルダがまだ無い場合でも、ボタン一発で保存できるように作成しておく。
		std::filesystem::path path(filePath);
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
		}

		// JSON には「復元に必要な軽い情報」だけを書く。
		// GPUリソースや Object3d は実行時リソースなので保存せず、Load 時に assetPath から再生成する。
		json root;
		root["name"] = path.stem().string();
		root["version"] = 2;
		root["objects"] = json::array();
		for (const SceneObject& sceneObject : sceneObjects_) {
			json item;
			item["type"] = "MESH";
			item["name"] = sceneObject.name;
			item["file_name"] = sceneObject.assetPath;
			if (sceneObject.disabled) {
				item["disabled"] = true;
			}
			// LevelDataLoaderと保存形式を共有するため、Y-up/ラジアンから
			// Blender互換のZ-up/度へ逆変換して書き出す。
			constexpr float kRadiansToDegrees = 180.0f / 3.1415926535f;
			const Vector3& position = sceneObject.transform.translate;
			const Vector3& rotation = sceneObject.transform.rotate;
			const Vector3& scale = sceneObject.transform.scale;
			item["transform"]["translation"] = Vector3ToJson({ position.x, position.z, position.y });
			item["transform"]["rotation"] = Vector3ToJson({
				-rotation.x * kRadiansToDegrees,
				 rotation.z * kRadiansToDegrees,
				-rotation.y * kRadiansToDegrees });
			item["transform"]["scaling"] = Vector3ToJson({ scale.x, scale.z, scale.y });
			if (sceneObject.collider.enabled) {
				item["collider"]["type"] = sceneObject.collider.type;
				item["collider"]["center"] = Vector3ToJson({
					sceneObject.collider.center.x, sceneObject.collider.center.z, sceneObject.collider.center.y });
				item["collider"]["size"] = Vector3ToJson({
					sceneObject.collider.size.x, sceneObject.collider.size.z, sceneObject.collider.size.y });
			}
			if (sceneObject.spawnPoint.enabled) {
				item["spawn_point"]["enabled"] = true;
				item["spawn_point"]["type"] = sceneObject.spawnPoint.type;
			}
			root["objects"].push_back(item);
		}

		std::ofstream file(filePath);
		if (!file.is_open()) {
			sceneEditorStatus_ = "Scene save failed: cannot open file.";
			return false;
		}
		file << root.dump(4);
		sceneEditorStatus_ = "Scene saved: " + filePath;
		return true;
	}
	catch (const std::exception& e) {
		sceneEditorStatus_ = std::string("Scene save failed: ") + e.what();
		return false;
	}
}

bool SkinningEditorController::LoadSceneObjects(const std::string& filePath) {
	try {
		std::ifstream file(filePath);
		if (!file.is_open()) {
			sceneEditorStatus_ = "Scene load failed: cannot open file.";
			return false;
		}

		json root;
		file >> root;
		// 課題用のversion 2、またはBlender出力JSONは共通ローダーで復元する。
		if (root.contains("name") && root.contains("objects") && root.at("objects").is_array() &&
			!root.at("objects").empty() && root.at("objects").front().contains("type")) {
			file.close();
			return LoadLevelDataIntoScene(filePath);
		}
		const json& objects = root.value("objects", json::array());
		// ロードは現在の配置を置き換える動作にする。
		// 既存オブジェクトと混ぜると、保存内容と画面状態が一致しづらいため。
		sceneObjects_.clear();
		selectedSceneObjectIndex_ = -1;

		for (const json& item : objects) {
			const std::string assetPath = item.value("assetPath", "");
			std::string directory;
			std::string fileName;

			// 保存済み Transform を読み戻し、assetPath から Model/Object3d を再生成する。
			// 参照先アセットが消えていた場合は、その1件だけスキップして残りのロードを続ける。
			SceneObject sceneObject;
			sceneObject.name = item.value("name", assetPath.empty() ? std::string("Scene Object") : GetDisplayFileName(assetPath, assetPath));
			sceneObject.assetPath = assetPath;
			sceneObject.disabled = item.value("disabled", false);
			sceneObject.transform.translate =
				JsonToVector3(item.value("position", json::array()), { 0.0f, 0.0f, 0.0f });
			sceneObject.transform.rotate =
				JsonToVector3(item.value("rotation", json::array()), { 0.0f, 0.0f, 0.0f });
			sceneObject.transform.scale =
				JsonToVector3(item.value("scale", json::array()), { 1.0f, 1.0f, 1.0f });
			if (item.contains("collider") && item.at("collider").is_object()) {
				const json& collider = item.at("collider");
				sceneObject.collider.enabled = true;
				sceneObject.collider.type = collider.value("type", "BOX");
				sceneObject.collider.center =
					JsonToVector3(collider.value("center", json::array()), { 0.0f, 0.0f, 0.0f });
				sceneObject.collider.size =
					JsonToVector3(collider.value("size", json::array()), { 2.0f, 2.0f, 2.0f });
			}
			if (item.contains("spawn_point") && item.at("spawn_point").is_object()) {
				const json& spawnPoint = item.at("spawn_point");
				sceneObject.spawnPoint.enabled = spawnPoint.value("enabled", true);
				sceneObject.spawnPoint.type = spawnPoint.value("type", "Player");
			}

			if (!assetPath.empty()) {
				if (!SplitModelPath(assetPath, directory, fileName)) {
					continue;
				}
				sceneObject.model = Model::CreateFromOBJ(dxCommon_, directory, fileName, textureManager_);
				if (!sceneObject.model) {
					continue;
				}

				sceneObject.object = std::make_unique<Object3d>();
				sceneObject.object->Initialize(object3dCommon_);
				sceneObject.object->SetModel(sceneObject.model.get());
			}

			sceneObjects_.push_back(std::move(sceneObject));
		}

		if (!sceneObjects_.empty()) {
			selectedSceneObjectIndex_ = 0;
		}
		sceneEditorStatus_ = "Scene loaded: " + filePath;
		return true;
	}
	catch (const std::exception& e) {
		sceneEditorStatus_ = std::string("Scene load failed: ") + e.what();
		return false;
	}
}

bool SkinningEditorController::LoadLevelDataIntoScene(const std::string& filePath) {
	loadedLevelName_.clear();
	levelLoadTotalNodes_ = 0;
	levelLoadMeshNodes_ = 0;
	levelLoadPlacedObjects_ = 0;
	levelLoadFailedObjects_ = 0;
	levelLoadSkippedObjects_ = 0;
	levelLoadMessages_.clear();

	if (!object3dCommon_ || !dxCommon_ || !textureManager_) {
		sceneEditorStatus_ = "Level load failed: editor resources are not initialized.";
		levelLoadMessages_.push_back(sceneEditorStatus_);
		return false;
	}

	LevelData levelData;
	std::string status;
	if (!LevelDataLoader::Load(filePath, levelData, &status)) {
		sceneEditorStatus_ = status;
		levelLoadMessages_.push_back(status);
		return false;
	}
	loadedLevelName_ = levelData.name;

	// External level loading replaces the current editor placement.
	// This keeps the viewport equal to the imported JSON instead of mixing old and new objects.
	sceneObjects_.clear();
	selectedSceneObjectIndex_ = -1;

	for (const LevelObjectData& objectData : levelData.objects) {
		AppendLevelObjectRecursive(objectData);
	}

	if (!sceneObjects_.empty()) {
		selectedSceneObjectIndex_ = 0;
	}

	sceneEditorStatus_ = "Level loaded: " + levelData.name +
		" placed " + std::to_string(levelLoadPlacedObjects_) +
		" / " + std::to_string(levelLoadMeshNodes_) + " mesh objects.";
	levelLoadMessages_.insert(levelLoadMessages_.begin(), sceneEditorStatus_);
	return levelLoadPlacedObjects_ > 0;
}

bool SkinningEditorController::AppendLevelObjectRecursive(const LevelObjectData& objectData) {
	bool placedAny = false;
	++levelLoadTotalNodes_;

	if (objectData.type == "MESH") {
		++levelLoadMeshNodes_;
		if (objectData.disabled) {
			++levelLoadSkippedObjects_;
			levelLoadMessages_.push_back("Skipped disabled: " + objectData.name);
		} else {
			const std::string resolvedModelPath = ResolveLevelModelPath(objectData.fileName);
			std::string directory;
			std::string fileName;
			if (!SplitModelPath(resolvedModelPath, directory, fileName)) {
				++levelLoadFailedObjects_;
				levelLoadMessages_.push_back("Failed: invalid model path [" + objectData.name + "]");
			} else if (!std::filesystem::exists(resolvedModelPath)) {
				++levelLoadFailedObjects_;
				levelLoadMessages_.push_back("Failed: missing file " + resolvedModelPath);
			} else {
				SceneObject sceneObject;
				sceneObject.name = objectData.name.empty()
					? GetDisplayFileName(resolvedModelPath, fileName)
					: objectData.name;
				sceneObject.assetPath = resolvedModelPath;
				sceneObject.transform = objectData.transform;
				sceneObject.disabled = objectData.disabled;
				sceneObject.collider = objectData.collider;
				sceneObject.spawnPoint = objectData.spawnPoint;
				sceneObject.model = Model::CreateFromOBJ(dxCommon_, directory, fileName, textureManager_);

				if (sceneObject.model) {
					sceneObject.object = std::make_unique<Object3d>();
					sceneObject.object->Initialize(object3dCommon_);
					sceneObject.object->SetModel(sceneObject.model.get());
					sceneObjects_.push_back(std::move(sceneObject));
					++levelLoadPlacedObjects_;
					placedAny = true;
					levelLoadMessages_.push_back("Placed: " + resolvedModelPath);
				} else {
					++levelLoadFailedObjects_;
					levelLoadMessages_.push_back("Failed: model load failed " + resolvedModelPath);
				}
			}
		}
	} else if (objectData.spawnPoint.enabled) {
		SceneObject sceneObject;
		sceneObject.name = objectData.name.empty() ? objectData.spawnPoint.type + " SpawnPoint" : objectData.name;
		sceneObject.assetPath = "";
		sceneObject.transform = objectData.transform;
		sceneObject.disabled = objectData.disabled;
		sceneObject.spawnPoint = objectData.spawnPoint;
		sceneObjects_.push_back(std::move(sceneObject));
		++levelLoadPlacedObjects_;
		placedAny = true;
		levelLoadMessages_.push_back("Placed spawn point: " + sceneObject.name);
	} else {
		++levelLoadSkippedObjects_;
		levelLoadMessages_.push_back("Skipped: " + objectData.type + " [" + objectData.name + "]");
	}

	// LevelDataLoader already bakes parent transforms into each child.
	// The editor keeps a flat SceneObject list because Object3d does not own a hierarchy yet.
	for (const LevelObjectData& child : objectData.children) {
		placedAny = AppendLevelObjectRecursive(child) || placedAny;
	}

	return placedAny;
}

// ==========================================================
//  SkinningEditorController::Draw
//  グリッド線・スキニングメッシュ・スケルトンを描画する
// ==========================================================
void SkinningEditorController::Draw(Object3dCommon* object3dCommon, Camera* camera) {
	// グリッド線の描画 (モードに関わらず常に表示)
	for (auto& line : gridLines_) {
		line->Draw();
	}

	for (auto& sceneObject : sceneObjects_) {
		if (!sceneObject.disabled && sceneObject.object) {
			sceneObject.object->Draw();
		}
	}

	if (isObjPreviewMode_ && objPreviewObject_) {
		// OBJ モード: 通常の Object3d として描画 (スケルトンなし)
		objPreviewObject_->Draw();

	} else if (skinnedObject_) {
		// glTF / デフォルト人型モード: スキニングメッシュとスケルトンを描画
		skinnedObject_->Draw();
		skinnedObject_->DrawSkeleton(
			object3dCommon, debugCubeModel_.get(),
			camera->GetViewMatrix(), camera->GetProjectionMatrix());
	}
}

// ==========================================================
//  SkinningEditorController::DrawShadow
//  シャドウマップへの描画 (スキニングメッシュが影を落とすため)
// ==========================================================
void SkinningEditorController::DrawShadow(const Matrix4x4& lightVP) {
	for (auto& sceneObject : sceneObjects_) {
		if (!sceneObject.disabled && sceneObject.object) {
			sceneObject.object->DrawShadow(lightVP);
		}
	}

	if (isObjPreviewMode_ && objPreviewObject_) {
		// OBJ モード: 通常の Object3d で影描画
		objPreviewObject_->DrawShadow(lightVP);
	} else if (skinnedObject_) {
		// glTF / デフォルト人型モード
		skinnedObject_->DrawShadow(lightVP);
	}
}

// ==========================================================
//  SkinningEditorController::DrawImGuiTimeline
//  下パネル (Tools & Controls) に描画するタイムライン UI
// ==========================================================
void SkinningEditorController::DrawImGuiTimeline() {
	if (!skinnedObject_) { return; }

	// OBJ モードはスケルトン・タイムラインが存在しないため代替メッセージを表示
	if (isObjPreviewMode_) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[ OBJ Model - No Animation ]");
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
			"This model is a static OBJ and does not support\n"
			"skeletal animation or keyframe editing.\n\n"
			"To add animations, export from Blender as .gltf or .glb.");
		return;
	}

	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Animation Timeline ]");

	auto* model = skinnedObject_->GetModel();
	float  duration = model->GetMotionDuration();
	float  curTime = skinnedObject_->GetCurrentKeyframeTime();
	bool   playCustom = skinnedObject_->IsPlayCustomAnimation();

	// ----------------------------------------------------------
	// タイムラインシークスライダー (全幅)
	// ----------------------------------------------------------
	ImGui::PushItemWidth(-1.0f);
	if (ImGui::SliderFloat("##TimelineSlider", &curTime, 0.0f, duration,
						   "Current Time: %.2f sec / %.2f sec")) {
		skinnedObject_->SetCurrentKeyframeTime(curTime);
		if (!playCustom) {
			skinnedObject_->ApplyMotion(curTime); // 停止中はシークと同時に適用
		}
	}
	ImGui::PopItemWidth();

	// ----------------------------------------------------------
	// トラックのビジュアル描画 (キーフレームひし形・目盛り・再生カーソル)
	// ----------------------------------------------------------
	float     width = ImGui::GetContentRegionAvail().x;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2    cursorScreenPos = ImGui::GetCursorScreenPos();

	const float trackHeight = 22.0f;
	ImVec2 trackMin = cursorScreenPos;
	ImVec2 trackMax = ImVec2(trackMin.x + width, trackMin.y + trackHeight);

	// 背景トラック (暗いグレー)
	drawList->AddRectFilled(trackMin, trackMax, IM_COL32(40, 40, 42, 255), 4.0f);
	drawList->AddRect(trackMin, trackMax, IM_COL32(80, 80, 85, 255), 4.0f);

	// 目盛り (0.1 秒ごと / 0.5 秒は長め)
	for (float t = 0.0f; t <= duration; t += 0.1f) {
		float ratio = t / duration;
		float posX = trackMin.x + ratio * width;
		float lineLen = (std::fmod(t, 0.5f) < 0.01f || std::abs(t - duration) < 0.01f) ? 14.0f : 7.0f;
		drawList->AddLine(
			ImVec2(posX, trackMin.y),
			ImVec2(posX, trackMin.y + lineLen),
			IM_COL32(130, 130, 135, 255));
	}

	// キーフレームマーク (ひし形) の描画
	const auto& motionData = model->GetMotionData();
	std::vector<float> kfTimes;
	if (!motionData.jointAnimations.empty()) {
		for (const auto& kf : motionData.jointAnimations[0].keyframes) {
			kfTimes.push_back(kf.time);
		}
	}
	for (float kfTime : kfTimes) {
		float  ratio = kfTime / duration;
		float  posX = trackMin.x + ratio * width;
		ImVec2 center = ImVec2(posX, trackMin.y + trackHeight * 0.5f);
		float  r = 6.0f;
		// ひし形内部 (ゴールド)
		drawList->AddQuadFilled(
			ImVec2(center.x, center.y - r), ImVec2(center.x + r, center.y),
			ImVec2(center.x, center.y + r), ImVec2(center.x - r, center.y),
			IM_COL32(255, 196, 0, 255));
		// ひし形輪郭 (白)
		drawList->AddQuad(
			ImVec2(center.x, center.y - r), ImVec2(center.x + r, center.y),
			ImVec2(center.x, center.y + r), ImVec2(center.x - r, center.y),
			IM_COL32(255, 255, 255, 200));
	}

	// 再生時間カーソルの縦線 (赤)
	float  currentRatio = curTime / duration;
	float  cursorX = trackMin.x + currentRatio * width;
	drawList->AddLine(
		ImVec2(cursorX, trackMin.y - 3.0f),
		ImVec2(cursorX, trackMax.y + 3.0f),
		IM_COL32(255, 60, 60, 255), 2.5f);
	drawList->AddTriangleFilled(
		ImVec2(cursorX - 5.0f, trackMin.y - 3.0f),
		ImVec2(cursorX + 5.0f, trackMin.y - 3.0f),
		ImVec2(cursorX, trackMin.y + 4.0f),
		IM_COL32(255, 60, 60, 255));

	ImGui::Dummy(ImVec2(0.0f, trackHeight + 8.0f));

	// ----------------------------------------------------------
	// タイムライン詳細リスト (ジョイント別キーフレーム一覧)
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::BeginChild("KeyframeDetails", ImVec2(0, 0), true);
	ImGui::Columns(3, "TimelineColumns", false);
	ImGui::SetColumnWidth(0, 160.0f);
	ImGui::SetColumnWidth(1, width - 400.0f);
	ImGui::SetColumnWidth(2, 240.0f);

	// ヘッダー行
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Joint Name");
	ImGui::NextColumn();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Registered Keyframes (Click to jump / preview)");
	ImGui::NextColumn();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Current Trans / Rot (Euler)");
	ImGui::NextColumn();
	ImGui::Separator();

	// ジョイントごとの行
	auto& joints = model->GetJoints();
	for (size_t i = 0; i < motionData.jointAnimations.size(); ++i) {
		const auto& anim = motionData.jointAnimations[i];

		// 選択中ジョイントはゴールドで強調
		if (static_cast<int>(i) == skinnedObject_->GetSelectedJointIndex()) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s (Selected)", anim.name.c_str());
		} else {
			ImGui::Text("%s", anim.name.c_str());
		}
		ImGui::NextColumn();

		// キーフレームボタン (クリックで時間をシーク)
		if (anim.keyframes.empty()) {
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No keyframes registered.");
		} else {
			for (size_t k = 0; k < anim.keyframes.size(); ++k) {
				char btnLabel[64];
				sprintf_s(btnLabel, "%.2fs##%d_%d", anim.keyframes[k].time, (int)i, (int)k);
				if (ImGui::Button(btnLabel, ImVec2(48, 20))) {
					skinnedObject_->SetCurrentKeyframeTime(anim.keyframes[k].time);
					skinnedObject_->ApplyMotion(anim.keyframes[k].time);
				}
				ImGui::SameLine();
			}
		}
		ImGui::NextColumn();

		// 現在の Translation / Rotation (度数法) を表示
		if (i < joints.size()) {
			const float rad2deg = 180.0f / 3.14159265f;
			ImGui::Text("T:(%.1f, %.1f) R:(%.0f, %.0f, %.0f)",
				joints[i].translation.x, joints[i].translation.y,
				joints[i].rotation.x * rad2deg,
				joints[i].rotation.y * rad2deg,
				joints[i].rotation.z * rad2deg);
		}
		ImGui::NextColumn();
		ImGui::Separator();
	}
	ImGui::EndChild();
}

// ==========================================================
//  SkinningEditorController::DrawAssetBrowserPanel
//  Draws model assets scanned from Resources/Models in a Unity-like project panel.
// ==========================================================
void SkinningEditorController::DrawAssetBrowserPanel(Player* player, Model* defaultObjModel) {
	(void)player;
	(void)defaultObjModel;

	if (modelNames_.empty()) {
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No model assets found.");
		return;
	}

	ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Project Assets ]");
	ImGui::SameLine();
	ImGui::Checkbox("Grid", &assetBrowserGridView_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderInt("Tile", &assetTileSize_, 64, 128, "%d px");
	ImGui::SameLine();
	if (ImGui::Button("Rescan Models", ImVec2(120, 22))) {
		const int previousIndex = selectedModelIndex_;
		ScanGltfModels();
		const int maxIndex = static_cast<int>(modelPaths_.size()) - 1;
		ChangePreviewModel(std::clamp(previousIndex, 0, maxIndex));
		assetBrowserStatus_ = "Model assets rescanned.";
	}

	ImGui::Separator();
	ImGui::BeginChild("ModelAssetBrowser", ImVec2(0, 0), false);

	if (assetBrowserGridView_) {
		const float tileSize = static_cast<float>(assetTileSize_);
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const int rawColumnCount = static_cast<int>(availableWidth / (tileSize + spacing));
		const int columnCount = rawColumnCount > 1 ? rawColumnCount : 1;
		ImGui::Columns(columnCount, nullptr, false);

		for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i) {
			const bool selected = (i == selectedModelIndex_);
			const ImVec4 baseColor = GetModelAssetColor(i, objStartIndex_, gltfStartIndex_);
			const std::string fileName = GetDisplayFileName(modelPaths_[i], modelNames_[i]);
			const bool hasThumbnail =
				i < static_cast<int>(assetHasThumbnail_.size()) &&
				i < static_cast<int>(assetThumbnailHandles_.size()) &&
				assetHasThumbnail_[i] &&
				textureManager_;
			const char* kind = GetModelAssetDisplayKind(i, objStartIndex_, gltfStartIndex_, hasThumbnail);

			ImGui::PushID(i);
			ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.95f, 0.72f, 0.30f, 1.0f) : baseColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(baseColor.x + 0.08f, baseColor.y + 0.08f, baseColor.z + 0.08f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(baseColor.x * 0.85f, baseColor.y * 0.85f, baseColor.z * 0.85f, 1.0f));

			bool clicked = false;
			if (hasThumbnail) {
				D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = assetThumbnailHandles_[i];
				ImTextureID textureId =
					reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(gpuHandle.ptr));
				clicked = DrawTexturedModelTile(
					textureId,
					ImVec2(tileSize, tileSize - 24.0f),
					selected,
					kind);
			} else {
				clicked = ImGui::Button(kind, ImVec2(tileSize, tileSize - 24.0f));
			}

			if (clicked) {
				ChangePreviewModel(i);
				assetBrowserStatus_ = "Preview: " + fileName;
			}
			ImGui::PopStyleColor(3);

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ImGui::SetDragDropPayload("MODEL_ASSET_INDEX", &i, sizeof(int));
				ImGui::Text("Model: %s", fileName.c_str());
				ImGui::EndDragDropSource();
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				ChangePreviewModel(i);
				if (PlaceAssetInScene(i)) {
					assetBrowserStatus_ = "Placed in scene: " + fileName;
				} else {
					assetBrowserStatus_ = sceneEditorStatus_;
				}
			}

			ImGui::TextWrapped("%s", fileName.c_str());
			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);
	} else {
		for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i) {
			const bool selected = (i == selectedModelIndex_);
			const std::string fileName = GetDisplayFileName(modelPaths_[i], modelNames_[i]);
			const bool hasThumbnail =
				i < static_cast<int>(assetHasThumbnail_.size()) &&
				assetHasThumbnail_[i];
			const std::string rowText =
				std::string(GetModelAssetDisplayKind(i, objStartIndex_, gltfStartIndex_, hasThumbnail)) +
				"  " + fileName;

			ImGui::PushID(i);
			if (ImGui::Selectable(rowText.c_str(), selected)) {
				ChangePreviewModel(i);
				assetBrowserStatus_ = "Preview: " + fileName;
			}
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				ImGui::SetDragDropPayload("MODEL_ASSET_INDEX", &i, sizeof(int));
				ImGui::Text("Model: %s", fileName.c_str());
				ImGui::EndDragDropSource();
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (PlaceAssetInScene(i)) {
					assetBrowserStatus_ = "Placed in scene: " + fileName;
				} else {
					assetBrowserStatus_ = sceneEditorStatus_;
				}
			}
			ImGui::PopID();
		}
	}

	if (!assetBrowserStatus_.empty()) {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "%s", assetBrowserStatus_.c_str());
	}

	ImGui::EndChild();
}

void SkinningEditorController::DrawSceneObjectPanel() {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Scene Objects ]");

	// Assets ブラウザで選択している OBJ を、そのまま独立した配置物としてシーンに追加する。
	// プレイヤー差し替えとは別機能なので、ここでは Player には触らない。
	if (ImGui::Button("Place Selected Asset", ImVec2(-FLT_MIN, 24.0f))) {
		PlaceSelectedAssetInScene();
	}

	// Save/Load は固定パスにせず、授業提出用や検証用に複数ファイルを作れるよう入力欄にしている。
	ImGui::InputText("Scene File", sceneFilePath_, IM_ARRAYSIZE(sceneFilePath_));
	const float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;
	if (ImGui::Button("Save Scene", ImVec2(halfWidth, 24.0f))) {
		SaveSceneObjects(sceneFilePath_);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Scene", ImVec2(-FLT_MIN, 24.0f))) {
		LoadSceneObjects(sceneFilePath_);
	}
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.55f, 0.20f, 1.0f));
	if (ImGui::Button("Save & Play Level", ImVec2(-FLT_MIN, 26.0f))) {
		// 保存に失敗した場合は古いJSONを誤って起動しない。
		playRequest_ = SaveSceneObjects(sceneFilePath_);
	}
	ImGui::PopStyleColor();
	ImGui::TextDisabled("F5: reload during play / F6: play saved level");
	if (ImGui::Button("Clear Scene Objects", ImVec2(-FLT_MIN, 22.0f))) {
		ClearSceneObjects();
	}

	if (!sceneEditorStatus_.empty()) {
		ImGui::TextWrapped("%s", sceneEditorStatus_.c_str());
	}

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ External Level Loader ]");
	ImGui::InputText("Level File", levelDataPath_, IM_ARRAYSIZE(levelDataPath_));
	if (ImGui::Button("Load Blender Level", ImVec2(-FLT_MIN, 24.0f))) {
		LoadLevelDataIntoScene(levelDataPath_);
	}
	ImGui::TextDisabled("Reads .json or level_editor.py .scene files with file_name / transform / collider.");

	if (!loadedLevelName_.empty() || !levelLoadMessages_.empty()) {
		if (ImGui::CollapsingHeader("Level Load Report", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Level: %s", loadedLevelName_.empty() ? "(none)" : loadedLevelName_.c_str());
			if (ImGui::BeginTable("##LevelLoadStats", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Visited Nodes");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%d", levelLoadTotalNodes_);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("MESH Nodes");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%d", levelLoadMeshNodes_);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Placed");
				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.55f, 1.0f), "%d", levelLoadPlacedObjects_);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Failed");
				ImGui::TableSetColumnIndex(1);
				ImGui::TextColored(
					levelLoadFailedObjects_ > 0
						? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
						: ImVec4(0.75f, 0.75f, 0.75f, 1.0f),
					"%d",
					levelLoadFailedObjects_);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted("Skipped");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%d", levelLoadSkippedObjects_);
				ImGui::EndTable();
			}

			if (ImGui::Button("Clear Load Report", ImVec2(-FLT_MIN, 22.0f))) {
				loadedLevelName_.clear();
				levelLoadTotalNodes_ = 0;
				levelLoadMeshNodes_ = 0;
				levelLoadPlacedObjects_ = 0;
				levelLoadFailedObjects_ = 0;
				levelLoadSkippedObjects_ = 0;
				levelLoadMessages_.clear();
			}

			if (!levelLoadMessages_.empty()) {
				if (ImGui::BeginChild("##LevelLoadMessages", ImVec2(-FLT_MIN, 92.0f), true)) {
					for (const std::string& message : levelLoadMessages_) {
						ImGui::TextWrapped("%s", message.c_str());
					}
				}
				ImGui::EndChild();
			}
		}
	}

	// 配置済みオブジェクトの一覧。選択したものだけ下の Transform 編集対象になる。
	// ここでは名前と番号だけを表示し、細かい情報は選択後の詳細欄に出す。
	ImGui::Text("Objects: %d", static_cast<int>(sceneObjects_.size()));
	if (ImGui::BeginListBox("##SceneObjectList", ImVec2(-FLT_MIN, 96.0f))) {
		for (int i = 0; i < static_cast<int>(sceneObjects_.size()); ++i) {
			const bool selected = i == selectedSceneObjectIndex_;
			std::string label = std::to_string(i) + ": " + sceneObjects_[i].name;
			if (ImGui::Selectable(label.c_str(), selected)) {
				selectedSceneObjectIndex_ = i;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndListBox();
	}

	if (selectedSceneObjectIndex_ < 0 ||
		selectedSceneObjectIndex_ >= static_cast<int>(sceneObjects_.size())) {
		ImGui::TextDisabled("Select a scene object to edit its transform.");
		return;
	}

	SceneObject& selectedObject = sceneObjects_[selectedSceneObjectIndex_];
	ImGui::Text("Selected: %s", selectedObject.name.c_str());
	ImGui::TextWrapped("Asset: %s", selectedObject.assetPath.empty() ? "(none)" : selectedObject.assetPath.c_str());
	ImGui::Checkbox("Disabled", &selectedObject.disabled);

	// Transform は SceneObject 側の保存用データを直接編集する。
	// 実際の Object3d への反映は Update() 内で毎フレーム同期する。
	Transform& transform = selectedObject.transform;
	ImGui::DragFloat3("Position", &transform.translate.x, 0.05f, -50.0f, 50.0f, "%.2f");
	ImGui::DragFloat3("Rotation", &transform.rotate.x, 0.02f, -6.28318f, 6.28318f, "%.2f rad");
	ImGui::DragFloat3("Scale", &transform.scale.x, 0.02f, 0.01f, 20.0f, "%.2f");

	if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Enabled", &selectedObject.collider.enabled);
		if (selectedObject.collider.enabled) {
			if (selectedObject.collider.type.empty()) {
				selectedObject.collider.type = "BOX";
			}
			char colliderType[32] = {};
			strncpy_s(colliderType, selectedObject.collider.type.c_str(), _TRUNCATE);
			if (ImGui::InputText("Type", colliderType, IM_ARRAYSIZE(colliderType))) {
				selectedObject.collider.type = colliderType;
			}
			ImGui::DragFloat3("Center", &selectedObject.collider.center.x, 0.05f, -50.0f, 50.0f, "%.2f");
			ImGui::DragFloat3("Size", &selectedObject.collider.size.x, 0.05f, 0.01f, 50.0f, "%.2f");
			if (ImGui::Button("Fit Collider To Mesh", ImVec2(-FLT_MIN, 22.0f))) {
				Vector3 meshCenter{};
				Vector3 meshSize{};
				if (ReadObjLocalBounds(selectedObject.assetPath, meshCenter, meshSize)) {
					// SceneObjectはエンジン座標のOBJを直接描画するため、ここでは軸変換せず格納する。
					// ワールド拡縮・回転はBuildWorldCollisionBoxesで描画と同じTransformを適用する。
					selectedObject.collider.center = meshCenter;
					selectedObject.collider.size = meshSize;
					sceneEditorStatus_ = "Collider fitted to OBJ bounds: " + selectedObject.name;
				} else {
					sceneEditorStatus_ = "Collider fit failed: OBJ vertices could not be read.";
				}
			}
		}
	}

	if (ImGui::CollapsingHeader("SpawnPoint")) {
		ImGui::Checkbox("Spawn Enabled", &selectedObject.spawnPoint.enabled);
		if (selectedObject.spawnPoint.enabled) {
			if (selectedObject.spawnPoint.type.empty()) {
				selectedObject.spawnPoint.type = "Player";
			}
			char spawnType[64] = {};
			strncpy_s(spawnType, selectedObject.spawnPoint.type.c_str(), _TRUNCATE);
			if (ImGui::InputText("Spawn Type", spawnType, IM_ARRAYSIZE(spawnType))) {
				selectedObject.spawnPoint.type = spawnType;
			}
			ImGui::TextDisabled("Position uses this object's Transform.");
		}
	}

	// よく使う調整だけボタン化して、手入力の手間を減らす。
	if (ImGui::Button("Snap To Ground", ImVec2(halfWidth, 22.0f))) {
		transform.translate.y = 0.0f;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Transform", ImVec2(-FLT_MIN, 22.0f))) {
		transform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	}

	// 複製は選択中 SceneObject の assetPath と Transform を元にして新しく作る。
	// Object3d は共有せず、Model も読み直して所有関係を単純に保つ。
	if (ImGui::Button("Duplicate", ImVec2(halfWidth, 22.0f))) {
		const std::string path = selectedObject.assetPath;
		const Transform duplicateTransform = selectedObject.transform;
		std::string directory;
		std::string fileName;
		if (SplitModelPath(path, directory, fileName)) {
			SceneObject duplicate;
			duplicate.name = selectedObject.name + " Copy";
			duplicate.assetPath = path;
			duplicate.transform = duplicateTransform;
			duplicate.collider = selectedObject.collider;
			duplicate.transform.translate.x += 1.0f;
			duplicate.model = Model::CreateFromOBJ(dxCommon_, directory, fileName, textureManager_);
			if (duplicate.model) {
				duplicate.object = std::make_unique<Object3d>();
				duplicate.object->Initialize(object3dCommon_);
				duplicate.object->SetModel(duplicate.model.get());
				sceneObjects_.push_back(std::move(duplicate));
				selectedSceneObjectIndex_ = static_cast<int>(sceneObjects_.size()) - 1;
				sceneEditorStatus_ = "Duplicated scene object.";
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete Selected", ImVec2(-FLT_MIN, 22.0f))) {
		sceneObjects_.erase(sceneObjects_.begin() + selectedSceneObjectIndex_);
		if (sceneObjects_.empty()) {
			selectedSceneObjectIndex_ = -1;
		} else {
			selectedSceneObjectIndex_ =
				std::clamp(selectedSceneObjectIndex_, 0, static_cast<int>(sceneObjects_.size()) - 1);
		}
		sceneEditorStatus_ = "Deleted scene object.";
	}
}

bool SkinningEditorController::ReloadExternalLevel(const std::string& filePath) {
	if (filePath.empty()) {
		return false;
	}
	strncpy_s(levelDataPath_, filePath.c_str(), _TRUNCATE);
	strncpy_s(sceneFilePath_, filePath.c_str(), _TRUNCATE);
	return LoadLevelDataIntoScene(filePath);
}

// ==========================================================
//  SkinningEditorController::DrawImGuiSidePanel
//  右パネル (Skinning Editor) の内容を描画する
// ==========================================================
void SkinningEditorController::DrawImGuiSidePanel(Camera* camera, Player* player, Model* defaultObjModel) {
	if (!skinnedObject_) { return; }

	// ----------------------------------------------------------
	// [ Model Selection ] モデル選択コンボボックス
	// ----------------------------------------------------------
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Model Selection ]");

	{
		// modelNames_ の char* 配列を作成してコンボに渡す
		std::vector<const char*> modelNamePtrs;
		for (const auto& name : modelNames_) {
			modelNamePtrs.push_back(name.c_str());
		}

		if (!modelNamePtrs.empty()) {
			int tempIdx = selectedModelIndex_;
			if (ImGui::Combo("##ModelList", &tempIdx,
							 modelNamePtrs.data(), static_cast<int>(modelNamePtrs.size()))) {
				if (tempIdx != selectedModelIndex_) {
					ChangePreviewModel(tempIdx);
				}
			}
		}
	}

	// ゲームに反映ボタン (緑)
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.55f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.75f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.4f, 0.15f, 1.0f));
	if (ImGui::Button("Apply to Game Player", ImVec2(-FLT_MIN, 26))) {
		ApplyModelToPlayer(player, defaultObjModel);
	}
	ImGui::PopStyleColor(3);

	// OBJ モード中はスキニング操作が使えない旨を表示
	if (isObjPreviewMode_) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
			"[OBJ Mode] No skeleton / animation.");
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
			"Import a .gltf/.glb for rigging.");
	}

	// 現在ゲームに適用中のモデル名を緑で表示
	if (activeGameModelIndex_ >= 0 && activeGameModelIndex_ < static_cast<int>(modelNames_.size())) {
		ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
						   "Active: %s", modelNames_[activeGameModelIndex_].c_str());
	}

	if (selectedModelIndex_ >= 0 && selectedModelIndex_ < static_cast<int>(modelPaths_.size())) {
		const std::string fileName = GetDisplayFileName(modelPaths_[selectedModelIndex_], modelNames_[selectedModelIndex_]);
		const bool selectedHasThumbnail =
			selectedModelIndex_ < static_cast<int>(assetHasThumbnail_.size()) &&
			assetHasThumbnail_[selectedModelIndex_];
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Asset Inspector ]");
		ImGui::Text("Name: %s", fileName.c_str());
		ImGui::Text("Type: %s", GetModelAssetDisplayKind(
			selectedModelIndex_,
			objStartIndex_,
			gltfStartIndex_,
			selectedHasThumbnail));
		ImGui::TextWrapped("Path: %s", modelPaths_[selectedModelIndex_].c_str());
		if (selectedModelIndex_ < static_cast<int>(assetHasThumbnail_.size()) &&
			assetHasThumbnail_[selectedModelIndex_] &&
			textureManager_) {
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = assetThumbnailHandles_[selectedModelIndex_];
			ImTextureID textureId =
				reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(gpuHandle.ptr));
			ImGui::Image(textureId, ImVec2(140.0f, 140.0f));
		}

		if (ImGui::Button("Place In Scene", ImVec2(-FLT_MIN, 26.0f))) {
			PlaceSelectedAssetInScene();
		}

		ImGui::Button("Drop Model To Scene", ImVec2(-FLT_MIN, 34.0f));
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_ASSET_INDEX")) {
				const int droppedIndex = *static_cast<const int*>(payload->Data);
				if (droppedIndex >= 0 && droppedIndex < static_cast<int>(modelPaths_.size())) {
					ChangePreviewModel(droppedIndex);
					PlaceAssetInScene(droppedIndex);
					assetBrowserStatus_ = sceneEditorStatus_;
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::TextDisabled("Click to preview. Double-click, button, or drop to place static OBJ assets.");
	}

	DrawSceneObjectPanel();

	// ----------------------------------------------------------
	// [ Animation Selection ] アニメーション (モーション) 選択
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Animation Selection ]");

	{
		auto* previewModel = skinnedObject_->GetModel();
		if (previewModel) {
			const auto& motions = previewModel->GetMotions();
			if (!motions.empty()) {
				// モーション名リストの構築 (name フィールドが空なら "Motion_N" で代替)
				std::vector<std::string> motionNames;
				for (size_t i = 0; i < motions.size(); ++i) {
					motionNames.push_back(
						motions[i].name.empty()
						? ("Motion_" + std::to_string(i))
						: motions[i].name);
				}
				std::vector<const char*> motionNamePtrs;
				for (const auto& n : motionNames) {
					motionNamePtrs.push_back(n.c_str());
				}

				int currentAnimIdx = previewModel->GetActiveMotionIndex();
				if (currentAnimIdx < 0) { currentAnimIdx = 0; }

				if (ImGui::Combo("##AnimList", &currentAnimIdx,
								 motionNamePtrs.data(), static_cast<int>(motionNamePtrs.size()))) {
					previewModel->SetActiveMotionIndex(currentAnimIdx);
					// glTF アニメーション再生：Custom Animation ON / Test Animation OFF に切り替え
					skinnedObject_->SetPlayCustomAnimation(true);
					skinnedObject_->SetPlayAnimation(false);
				}

				ImGui::Text("Total Motions: %d", static_cast<int>(motions.size()));
				if (currentAnimIdx >= 0 && currentAnimIdx < static_cast<int>(motions.size())) {
					ImGui::Text("Duration: %.2f sec", motions[currentAnimIdx].duration);
				}

				// Animation blend preview. A/B poses are evaluated at the same time and blended each frame.
				if (blendTargetMotionIndex_ < 0 || blendTargetMotionIndex_ >= static_cast<int>(motions.size())) {
					blendTargetMotionIndex_ = currentAnimIdx;
				}
				ImGui::Combo("Blend Target", &blendTargetMotionIndex_,
							 motionNamePtrs.data(), static_cast<int>(motionNamePtrs.size()));
				ImGui::SliderFloat("Blend Duration", &blendDuration_, 0.05f, 2.0f, "%.2f sec");
				if (ImGui::Button("Blend To Target", ImVec2(-FLT_MIN, 24))) {
					skinnedObject_->StartMotionBlend(blendTargetMotionIndex_, blendDuration_);
					motionStatus_ = "Animation blend started";
				}
				if (skinnedObject_->IsMotionBlending()) {
					ImGui::Text("Blend Rate: %.2f", skinnedObject_->GetBlendRate());
				}
			} else {
				ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "No animations in this model.");
			}
		}
	}

	// ----------------------------------------------------------
	// [ Skinned Mesh Settings ] スキニングメッシュ設定
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Skinned Mesh Settings ]");

	bool playAnim = skinnedObject_->IsPlayAnimation();
	if (ImGui::Checkbox("Play Test Animation", &playAnim)) {
		skinnedObject_->SetPlayAnimation(playAnim);
	}

	float speed = skinnedObject_->GetAnimationSpeed();
	if (ImGui::SliderFloat("Anim Speed", &speed, 0.0f, 3.0f, "%.2f")) {
		skinnedObject_->SetAnimationSpeed(speed);
	}

	bool showSkeleton = skinnedObject_->IsShowSkeleton();
	if (ImGui::Checkbox("Show Skeleton Bones", &showSkeleton)) {
		skinnedObject_->SetShowSkeleton(showSkeleton);
	}

	bool showJointAxes = skinnedObject_->IsShowJointAxes();
	if (ImGui::Checkbox("Show Selected Bone Axes", &showJointAxes)) {
		skinnedObject_->SetShowJointAxes(showJointAxes);
	}

	// 手ジョイントの位置をエミッターとして使う評価課題用の確認機能。
	if (ImGui::Checkbox("Emit Particles From Hand", &emitHandParticles_)) {
		handParticleTimer_ = 0.0f;
		handParticleJointIndex_ = -1;
	}
	if (emitHandParticles_) {
		const int resolvedJoint = handParticleJointIndex_ >= 0
			? handParticleJointIndex_
			: skinnedObject_->FindJointIndexByNameHints({
				"hand_r", "r_hand", "right_hand", "righthand", "hand.r",
				"hand_l", "l_hand", "left_hand", "lefthand", "hand.l", "hand"
			});
		if (resolvedJoint >= 0 && resolvedJoint < static_cast<int>(skinnedObject_->GetModel()->GetJoints().size())) {
			ImGui::Text("Emitter Joint: %s", skinnedObject_->GetModel()->GetJoints()[resolvedJoint].name.c_str());
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "Emitter Joint: not found");
		}
	}

	if (ImGui::Button("Reset to T-Pose", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->GetModel()->ResetPose();
	}

	// ----------------------------------------------------------
	// [ Camera Presets ] Blender スタイルのカメラプリセット
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Camera Presets (Blender Style) ]");

	float halfW = ImGui::GetContentRegionAvail().x * 0.5f;
	if (ImGui::Button("Focus Model", ImVec2(halfW, 24))) {
		camera->SetTarget({ 0.0f, 1.0f, 0.0f });
		camera->SetDistance(3.5f);
		camera->SetRotation({ 0.1f, 0.0f, 0.0f }); // ほぼ正面
	}
	ImGui::SameLine();
	if (ImGui::Button("Front View", ImVec2(-FLT_MIN, 24))) {
		camera->SetTarget({ 0.0f, 1.0f, 0.0f });
		camera->SetDistance(3.5f);
		camera->SetRotation({ 0.0f, 0.0f, 0.0f }); // 完全正面
	}
	if (ImGui::Button("Side View", ImVec2(halfW, 24))) {
		camera->SetTarget({ 0.0f, 1.0f, 0.0f });
		camera->SetDistance(3.5f);
		camera->SetRotation({ 0.0f, 1.5708f, 0.0f }); // 右横
	}
	ImGui::SameLine();
	if (ImGui::Button("Top View", ImVec2(-FLT_MIN, 24))) {
		camera->SetTarget({ 0.0f, 1.0f, 0.0f });
		camera->SetDistance(3.5f);
		camera->SetRotation({ 1.5708f, 0.0f, 0.0f }); // 真上
	}

	// ----------------------------------------------------------
	// [ Custom Motion Editor ] 手動キーフレームエディタ
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Editor ]");

	bool playCustom = skinnedObject_->IsPlayCustomAnimation();
	if (ImGui::Checkbox("Play Custom Motion", &playCustom)) {
		skinnedObject_->SetPlayCustomAnimation(playCustom);
		if (playCustom) {
			skinnedObject_->SetPlayAnimation(false); // テストアニメーションと排他
		}
	}

	if (ImGui::InputText("Motion Name", motionName_, IM_ARRAYSIZE(motionName_))) {
		skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
	}

	float duration = skinnedObject_->GetModel()->GetMotionDuration();
	if (ImGui::InputFloat("Motion Duration", &duration, 0.1f, 1.0f, "%.2f")) {
		if (duration < 0.1f) { duration = 0.1f; }
		skinnedObject_->GetModel()->SetMotionDuration(duration);
	}

	float curTime = skinnedObject_->GetCurrentKeyframeTime();
	if (ImGui::SliderFloat("Timeline Time", &curTime, 0.0f, duration, "%.2f sec")) {
		skinnedObject_->SetCurrentKeyframeTime(curTime);
		if (!playCustom) {
			skinnedObject_->ApplyMotion(curTime);
		}
	}

	if (ImGui::Button("Add Keyframe (Current Pose)", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->AddKeyframe(curTime);
		motionStatus_ = "Keyframe added at " + std::to_string(curTime) + " sec";
	}
	if (ImGui::Button("Clear All Keyframes", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->ClearKeyframes();
		motionStatus_ = "Cleared all keyframes";
	}
	if (ImGui::Button("New Empty Motion", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->ClearKeyframes();
		skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
		skinnedObject_->SetCurrentKeyframeTime(0.0f);
		skinnedObject_->ApplyMotion(0.0f);
		motionStatus_ = "Started a new empty motion";
	}
	if (ImGui::Button("Generate Walk Preset", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->GenerateWalkPreset();
		strncpy_s(motionName_, "WalkPreset", _TRUNCATE);
		skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
		motionStatus_ = "Generated walk preset";
	}
	if (ImGui::Button("Generate Run Preset", ImVec2(-FLT_MIN, 24))) {
		skinnedObject_->GenerateRunPreset();
		strncpy_s(motionName_, "RunPreset", _TRUNCATE);
		skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
		motionStatus_ = "Generated run preset";
	}

	ImGui::InputText("Motion Path", motionPath_, IM_ARRAYSIZE(motionPath_));

	float saveLoadW = ImGui::GetContentRegionAvail().x * 0.5f;
	if (ImGui::Button("Save Motion to File", ImVec2(saveLoadW, 24))) {
		skinnedObject_->GetModel()->SetActiveMotionName(motionName_);
		if (skinnedObject_->SaveMotion(motionPath_)) {
			hasCustomMotionFile_ = true;
			motionStatus_ = std::string("Saved: ") + motionPath_;
		} else {
			motionStatus_ = std::string("Save failed: ") + motionPath_;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Motion from File", ImVec2(-FLT_MIN, 24))) {
		if (skinnedObject_->LoadMotion(motionPath_)) {
			hasCustomMotionFile_ = true;
			const std::string& loadedName = skinnedObject_->GetModel()->GetActiveMotionName();
			strncpy_s(motionName_, loadedName.c_str(), _TRUNCATE);
			skinnedObject_->SetCurrentKeyframeTime(0.0f);
			motionStatus_ = std::string("Loaded: ") + motionPath_;
		} else {
			motionStatus_ = std::string("Load failed: ") + motionPath_;
		}
	}
	if (!motionStatus_.empty()) {
		ImGui::TextWrapped("%s", motionStatus_.c_str());
	}

	// ----------------------------------------------------------
	// [ Bone Transformations ] ボーン選択と回転・平行移動の手動調整
	// ----------------------------------------------------------
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Bone Transformations ]");

	auto& joints = skinnedObject_->GetModel()->GetJoints();
	int   selectedJoint = skinnedObject_->GetSelectedJointIndex();

	// ジョイント名のドロップダウンリスト
	std::vector<const char*> jointNames;
	for (const auto& j : joints) {
		jointNames.push_back(j.name.c_str());
	}
	if (ImGui::Combo("Select Bone", &selectedJoint,
					 jointNames.data(), static_cast<int>(jointNames.size()))) {
		skinnedObject_->SetSelectedJointIndex(selectedJoint);
	}

	if (selectedJoint >= 0 && selectedJoint < static_cast<int>(joints.size())) {
		auto& joint = joints[selectedJoint];

		ImGui::Text("Index: %d | Parent: %d | Children: %d",
					selectedJoint,
					joint.parentIndex,
					static_cast<int>(joint.childIndices.size()));
		ImGui::Separator();

		// 回転スライダー (ラジアン ↔ 度数法 で変換して表示)
		const float deg2rad = 3.14159265f / 180.0f;
		const float rad2deg = 180.0f / 3.14159265f;
		Vector3 rotDeg = {
			joint.rotation.x * rad2deg,
			joint.rotation.y * rad2deg,
			joint.rotation.z * rad2deg
		};

		ImGui::Text("Rotation (Degrees):");
		if (ImGui::SliderFloat("Rot X", &rotDeg.x, -180.0f, 180.0f, "%.1f")) {
			joint.rotation.x = rotDeg.x * deg2rad;
		}
		if (ImGui::SliderFloat("Rot Y", &rotDeg.y, -180.0f, 180.0f, "%.1f")) {
			joint.rotation.y = rotDeg.y * deg2rad;
		}
		if (ImGui::SliderFloat("Rot Z", &rotDeg.z, -180.0f, 180.0f, "%.1f")) {
			joint.rotation.z = rotDeg.z * deg2rad;
		}

		ImGui::Separator();
		ImGui::Text("Translation Offset:");
		ImGui::DragFloat3("Translate", &joint.translation.x, 0.01f, -2.0f, 2.0f, "%.3f");

		ImGui::Text("Scale:");
		ImGui::DragFloat3("Scale", &joint.scale.x, 0.01f, 0.1f, 5.0f, "%.3f");
	} else {
		ImGui::Text("No bone selected.");
	}
}

// ==========================================================
//  SkinningEditorController::ScanGltfModels  [private]
//  Resources/Models 以下の .gltf/.glb ファイルをスキャンしてリストに追加する
// ==========================================================
void SkinningEditorController::ScanGltfModels() {
	modelPaths_.clear();
	modelNames_.clear();
	assetThumbnailHandles_.clear();
	assetHasThumbnail_.clear();

	auto appendModelAsset = [this](const std::string& displayName, const std::string& modelPath) {
		modelNames_.push_back(displayName);
		modelPaths_.push_back(modelPath);

		const std::string thumbnailPath = FindSidecarThumbnailPath(modelPath);
		if (!thumbnailPath.empty() && textureManager_ && dxCommon_) {
			const uint32_t textureHandle = textureManager_->LoadTexture(thumbnailPath);
			D3D12_GPU_DESCRIPTOR_HANDLE imguiHandle = {};
			auto cached = assetThumbnailCache_.find(textureHandle);
			if (cached != assetThumbnailCache_.end()) {
				imguiHandle = cached->second;
			} else {
				imguiHandle = dxCommon_->RegisterImGuiTexture(
					textureManager_->GetResource(textureHandle),
					textureManager_->GetResourceDesc(textureHandle));
				if (imguiHandle.ptr != 0) {
					assetThumbnailCache_[textureHandle] = imguiHandle;
				}
			}

			assetThumbnailHandles_.push_back(imguiHandle);
			assetHasThumbnail_.push_back(imguiHandle.ptr != 0);
		} else {
			assetThumbnailHandles_.push_back({});
			assetHasThumbnail_.push_back(false);
		}
		};

	// ----------------------------------------------------------
	// インデックス 0 : デフォルト人型 (組み込みスキニング)
	// ----------------------------------------------------------
	appendModelAsset("Default Humanoid (Skinning)", "Default");

	// ----------------------------------------------------------
	// インデックス 1以降 : Resources/Models 以下の OBJ を再帰スキャン
	//   OBJ は静止モデルとして Object3d で表示する
	// ----------------------------------------------------------
	objStartIndex_ = static_cast<int>(modelPaths_.size()); // OBJ の開始位置を記録
	const std::string modelsDir = "Resources/Models";
	if (std::filesystem::exists(modelsDir)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
			if (!entry.is_regular_file()) { continue; }
			std::string ext = entry.path().extension().string();
			if (ext != ".obj") { continue; }

			std::string relPath = entry.path().string();
			std::replace(relPath.begin(), relPath.end(), '\\', '/');
			appendModelAsset("[OBJ] " + entry.path().filename().string(), relPath);
			// ファイル名だけ表示 (例: player.obj)
		}
	}

	// ----------------------------------------------------------
	// OBJ の後 : glTF / GLB を再帰スキャン
	//   glTF は SkinnedObject でアニメーション付き表示する
	// ----------------------------------------------------------
	gltfStartIndex_ = static_cast<int>(modelPaths_.size()); // glTF の開始位置を記録
	if (std::filesystem::exists(modelsDir)) {
		for (const auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
			if (!entry.is_regular_file()) { continue; }
			std::string ext = entry.path().extension().string();
			if (ext != ".gltf" && ext != ".glb") { continue; }

			std::string relPath = entry.path().string();
			std::replace(relPath.begin(), relPath.end(), '\\', '/');
			appendModelAsset("[glTF] " + entry.path().filename().string(), relPath);
		}
	}
}

// ==========================================================
//  SkinningEditorController::ChangePreviewModel  [private]
//  プレビュー SkinnedObject を指定インデックスのモデルで再初期化する
// ==========================================================
void SkinningEditorController::ChangePreviewModel(int index) {
	if (index < 0 || index >= static_cast<int>(modelPaths_.size())) {
		return;
	}
	selectedModelIndex_ = index;

	// モデルごとにジョイント名が違うため、プレビュー切り替え時に検索をやり直す。
	handParticleJointIndex_ = -1;
	handParticleTimer_ = 0.0f;

	if (index == 0) {
		// ----------------------------------------------------------
		// デフォルト人型 (組み込みスキニング) : SkinnedObject で表示
		// ----------------------------------------------------------
		isObjPreviewMode_ = false;
		skinnedObject_->Initialize(object3dCommon_, dxCommon_, textureManager_);

	} else if (index >= objStartIndex_ && index < gltfStartIndex_) {
		// ----------------------------------------------------------
		// OBJ モデル : Object3d + Model で表示 (スキニングなし)
		//   directoryPath と filename に分割して CreateFromOBJ に渡す
		// ----------------------------------------------------------
		isObjPreviewMode_ = true;

		// フルパスからディレクトリとファイル名を分離する
		std::string fullPath = modelPaths_[index];
		size_t lastSlash = fullPath.rfind('/');
		std::string dir = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash) : ".";
		std::string file = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

		// OBJ を Model としてロードして Object3d にセット
		objPreviewModel_ = std::unique_ptr<Model>(
			Model::CreateFromOBJ(dxCommon_, dir, file, textureManager_));

		objPreviewObject_ = std::make_unique<Object3d>();
		objPreviewObject_->Initialize(object3dCommon_);
		objPreviewObject_->SetModel(objPreviewModel_.get());
		objPreviewObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
		objPreviewObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		objPreviewObject_->SetRotation({ 0.0f, 0.0f, 0.0f });

	} else if (index >= gltfStartIndex_) {
		// ----------------------------------------------------------
		// glTF モデル : SkinnedObject でアニメーション付き表示
		// ----------------------------------------------------------
		isObjPreviewMode_ = false;
		skinnedObject_->InitializeFromGltf(
			object3dCommon_, dxCommon_, modelPaths_[index], textureManager_);
	}
}

// ==========================================================
//  SkinningEditorController::ApplyModelToPlayer  [private]
//  現在選択中のモデルをゲームプレイ用プレイヤーに反映する
// ==========================================================
void SkinningEditorController::ApplyModelToPlayer(Player* player, Model* defaultObjModel) {
	activeGameModelIndex_ = selectedModelIndex_;
	if (!player) { return; }

	if (activeGameModelIndex_ == 0) {
		// ----------------------------------------------------------
		// デフォルト人型スキニング
		// ----------------------------------------------------------
		appliedObjModel_.reset();
		player->InitializeWithDefaultSkinned(object3dCommon_, dxCommon_, textureManager_);

	} else if (activeGameModelIndex_ >= objStartIndex_ && activeGameModelIndex_ < gltfStartIndex_) {
		// ----------------------------------------------------------
		// OBJ モデルをプレイヤーに適用
		//   Player::Initialize() は Model* を受け取るため、
		//   プレビュー用の objPreviewModel_ をそのまま渡す
		//   (Player は Model の所有権を持たないので安全)
		// ----------------------------------------------------------
		std::string fullPath = modelPaths_[activeGameModelIndex_];
		size_t lastSlash = fullPath.rfind('/');
		std::string dir = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash) : ".";
		std::string file = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;

		appliedObjModel_ = std::unique_ptr<Model>(
			Model::CreateFromOBJ(dxCommon_, dir, file, textureManager_));

		if (appliedObjModel_) {
			player->Initialize(object3dCommon_, appliedObjModel_.get());
		} else {
			// フォールバック: デフォルト OBJ モデルを使用
			player->Initialize(object3dCommon_, defaultObjModel);
		}

	} else if (activeGameModelIndex_ >= gltfStartIndex_) {
		// ----------------------------------------------------------
		// glTF モデルをプレイヤーに適用
		// ----------------------------------------------------------
		appliedObjModel_.reset();
		player->InitializeWithSkinnedGltf(
			object3dCommon_, dxCommon_, modelPaths_[activeGameModelIndex_], textureManager_);
	}

	if (hasCustomMotionFile_ && player->IsSkinned() && player->GetSkinnedObject()) {
		if (player->GetSkinnedObject()->LoadMotion(motionPath_)) {
			player->GetSkinnedObject()->SetPlayAnimation(false);
			player->GetSkinnedObject()->SetPlayCustomAnimation(true);
			motionStatus_ = std::string("Applied model and motion to player: ") + motionPath_;
		} else {
			motionStatus_ = std::string("Applied model, but motion load failed: ") + motionPath_;
		}
	}

	// プレイヤーをデフォルトのスタート位置に配置
	player->SetPosition({ 0.0f, 1.5f, 0.0f });
}
