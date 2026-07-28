#include "StageEditorController.h"
#include "../Environment/WeatherPresetManager.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#ifdef USE_IMGUI
namespace {
	// エディタ用のウィンドウレイアウト情報をまとめる構造体
	struct StageEditorLayout {
		float rightPanelWidth = 320.0f;
		float toolbarHeight = 38.0f;
		float panelHeight = 1080.0f;
	};

	// ディスプレイサイズに応じて、エディタ用のウィンドウレイアウトを計算する。
	StageEditorLayout MakeStageEditorLayout(const ImVec2& displaySize) {
		StageEditorLayout layout;
		const float width = (std::max)(displaySize.x, 1.0f);
		const float height = (std::max)(displaySize.y, 1.0f);

		// 画面幅に応じて右側パネルの幅を調整する。最小300px、最大380pxの範囲でスケーリングする。
		float sidePanel = std::clamp(width * 0.20f, 320.0f, 400.0f);
		if (width < 1360.0f) {
			sidePanel = std::clamp(width * 0.25f, 280.0f, 340.0f);
		}

		layout.rightPanelWidth = sidePanel;
		layout.toolbarHeight = 38.0f;
		layout.panelHeight = (std::max)(300.0f, height - layout.toolbarHeight);
		return layout;
	}
}
#endif

void StageEditorController::Initialize() {
	LoadCampaignSequence();
	// ブロック表示スケールの初期化
	editorBlockScale_ = { 1.0f, 1.0f, 1.0f };
	editorUniformBlockScale_ = 1.0f;

	selectedStageIndex_ = -1;
	selectedBlockType_ = BlockType::Ground;
	bubbleInsideBlockType_ = BlockType::Wall;
	selectedCustomPartSlot_ = 1;
	bubbleInsideCustomSlot_ = 0;
	selectedTimedGroupId_ = 1;
	selectedTimedOrderId_ = 0;

	// ドアのペアリング状態の初期化
	isWaitingForSecondDoor_ = false;
	firstDoorIndex_ = { -1, -1, -1 };

	// 保存済みのステージ一覧をスキャン・更新
	RefreshStageList();
}

// Resources/Stages フォルダ内のステージファイル（.txt）一覧を再取得・更新します。
void StageEditorController::RefreshStageList() {
	stageFiles_.clear();
	// ステージファイルの格納ディレクトリ
	std::string path = "Resources/Stages/";

	// ディレクトリが存在しない場合は自動作成
	if (!std::filesystem::exists(path)) {
		std::filesystem::create_directories(path);
	}

	// フォルダ内の .txt ファイルを列挙してリストに格納（拡張子を除くファイル名）
	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.path().extension() == ".txt") {
			stageFiles_.push_back(entry.path().stem().string());
		}
	}
}

// ステージマップ全体を走査し、プレイヤースタートブロック（PlayerStart）の位置へプレイヤーをリセットします。
void StageEditorController::ResetPlayerToStartCell(StageMap& stageMap, Player* player) {
	if (!player) {
		return;
	}

	bool foundStart = false;

	// ステージマップ全体を走査して、PlayerStart ブロックを探す
	for (int y = 0; y < stageMap.GetHeight(); ++y) {
		for (int z = 0; z < stageMap.GetDepth(); ++z) {
			for (int x = 0; x < stageMap.GetWidth(); ++x) {
				const MapCell* cell = stageMap.GetCell(x, y, z);

				// PlayerStart ブロックが見つかった場合、プレイヤーの位置とリスポーン位置を設定する
				if (cell && cell->type == BlockType::PlayerStart) {
					Vector3 startPos = {
						static_cast<float>(x),
						static_cast<float>(y) + 1.1f,
						static_cast<float>(z)
					};

					player->SetPosition(startPos);
					player->SetRespawnPosition(startPos);

					foundStart = true;
					break;
				}
			}
			if (foundStart) {
				break;
			}
		}

		// PlayerStart ブロックが見つかった場合、外側のループも抜ける
		if (foundStart) {
			break;
		}
	}

	// PlayerStart ブロックが見つからなかった場合、デフォルト位置にリセットする
	if (!foundStart) {
		Vector3 defaultPos = { 0.0f, 1.5f, 0.0f };

		player->SetPosition(defaultPos);
		player->SetRespawnPosition(defaultPos);
	}
}

// 現在のカーソル位置に対して、選択中のブロック（またはドアのペアリング）を配置・適用します。
void StageEditorController::HandleCursorInput(Input* input, StageMap& stageMap, MapCursor* mapCursor, LightCamera* lightCamera, Camera* camera) {
	if (!input || !mapCursor || !lightCamera || !camera) {
		return;
	}

	float cameraRotY = camera->GetTransform().rotate.y;

	// 入力方向ベクトル
	int inputX = 0;
	int inputZ = 0;

	if (RepeatKey(input, DIK_A)) { inputX -= 1; }
	if (RepeatKey(input, DIK_D)) { inputX += 1; }
	if (RepeatKey(input, DIK_W)) { inputZ += 1; }
	if (RepeatKey(input, DIK_S)) { inputZ -= 1; }

	if (inputX != 0 || inputZ != 0) {
		// カメラの回転に基づいて移動ベクトルを計算
		float moveX = (float)inputX * std::cos(cameraRotY) + (float)inputZ * std::sin(cameraRotY);
		float moveZ = -(float)inputX * std::sin(cameraRotY) + (float)inputZ * std::cos(cameraRotY);

		// グリッド移動なので、絶対値が大きい方の軸へ移動する
		int dx = 0;
		int dz = 0;
		if (std::abs(moveX) > std::abs(moveZ)) {
			dx = moveX > 0.0f ? 1 : -1;
		}
		else {
			dz = moveZ > 0.0f ? 1 : -1;
		}

		mapCursor->Move(dx, 0, dz, stageMap);
	}

	if (RepeatKey(input, DIK_Q)) {
		mapCursor->Move(0, 1, 0, stageMap);
	}
	if (RepeatKey(input, DIK_E)) {
		mapCursor->Move(0, -1, 0, stageMap);
	}
	// 移動後のカーソル位置や描画用行列を更新
	mapCursor->Update(lightCamera->GetViewProjectionMatrix());
}

// IJKL / UO キーによるエディタカメラの平行移動・回転
void StageEditorController::HandleCameraInput(Input* input, Camera* camera) {
	if (!input || !camera) {
		return;
	}
	Transform& camTf = camera->GetTransform();

	// IJKL / UO キーによるエディタカメラの平行移動・回転
	if (input->PushKey(DIK_J)) {
		camTf.rotate.y += 0.02f;
	}
	if (input->PushKey(DIK_L)) {
		camTf.rotate.y -= 0.02f;
	}
	if (input->PushKey(DIK_I)) {
		camTf.translate.z += 0.2f;
	}
	if (input->PushKey(DIK_K)) {
		camTf.translate.z -= 0.2f;
	}
	if (input->PushKey(DIK_U)) {
		camTf.translate.y += 0.2f;
	}
	if (input->PushKey(DIK_O)) {
		camTf.translate.y -= 0.2f;
	}
}

// エディタモードにおける毎フレームの更新処理（キー入力、配置・削除判定など）を行います。
void StageEditorController::Update(Input* input, StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, LightCamera* lightCamera, Player* player, Camera* camera) {
	if (!input) {
		return;
	}

	// 長押しフレームの更新
	if (input->PushKey(DIK_RETURN) || input->PushKey(DIK_SPACE) ||
		input->PushKey(DIK_A) || input->PushKey(DIK_D) ||
		input->PushKey(DIK_W) || input->PushKey(DIK_S) ||
		input->PushKey(DIK_Q) || input->PushKey(DIK_E)) {
		holdFrame_++;
	}
	else {
		holdFrame_ = 0;
	}


	// 3. ブロック配置 (Enter)
	if (input->TriggerKey(DIK_RETURN) || RepeatKey(input, DIK_RETURN, 20, 5)) {
		ApplyPlacement(stageMap, stageRenderer, mapCursor, player);
	}

	// 4. ブロック削除 (Space)
	if (input->TriggerKey(DIK_SPACE) || RepeatKey(input, DIK_SPACE, 20, 5)) {
		if (mapCursor) {
			stageMap.RemoveBlock(mapCursor->GetIndex());
			if (stageRenderer) {
				stageRenderer->BuildFromStageMap(stageMap);
			}
		}
	}
	// 1. カーソル操作 (WASD / QE)
	HandleCursorInput(input, stageMap, mapCursor, lightCamera, camera);

	// 2. エディタカメラ操作 (IJKL / UO)
	HandleCameraInput(input, camera);

	// 5. ブロック回転 (Rキー)
	if (input->TriggerKey(DIK_R)) {
		if (mapCursor) {
			const Int3& cursor = mapCursor->GetIndex();
			MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
			if (cell && cell->type != BlockType::None) {
				cell->rotationY += 1.5708f;
				if (stageRenderer) {
					stageRenderer->BuildFromStageMap(stageMap);
				}
			}
		}
	}
}


// ImGui によるエディタ用パネル（セーブロード、設定、ツールバー）を描画します。
void StageEditorController::DrawImGui(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
#ifdef USE_IMGUI
	ImGuiIO& io = ImGui::GetIO();
	const StageEditorLayout layout = MakeStageEditorLayout(io.DisplaySize);

	// 右側パネルに配置：ステージの保存・読み込み管理
	ImGui::SetNextWindowPos(
		ImVec2(io.DisplaySize.x - layout.rightPanelWidth, layout.toolbarHeight),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		ImVec2(layout.rightPanelWidth, layout.panelHeight), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
	ImGui::Begin("Stage Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	// --- 天候・環境設定 UI ---
	if (ImGui::CollapsingHeader("Weather / Environment")) {
		auto& wpMgr = WeatherPresetManager::GetInstance();
		auto& presets = wpMgr.GetPresets();

		// プリセットが存在しない場合のメッセージ表示
		if (presets.empty()) {
			ImGui::Text("No presets available.");
		}
		else {
			// 現在のプリセット名を取得
			std::string currentPresetName = stageMap.GetWeatherPresetName();
			if (currentPresetName.empty()) {
				currentPresetName = presets[0].name;
				stageMap.SetWeatherPresetName(currentPresetName);
			}

			// ドロップダウン
			if (ImGui::BeginCombo("Preset", currentPresetName.c_str())) {
				for (const auto& p : presets) {
					bool isSelected = (currentPresetName == p.name);
					if (ImGui::Selectable(p.name.c_str(), isSelected)) {
						stageMap.SetWeatherPresetName(p.name);
						// 選んだ瞬間にパラメータをステージに反映
						stageMap.SetClearColor(p.clearColor);
						stageMap.SetLightIntensity(p.lightIntensity);
						stageMap.SetLightColor(p.lightColor);
						stageMap.SetLightDirection(p.lightDirection);
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			// 現在のプリセットを編集する
			WeatherPreset* currentPreset = wpMgr.GetPresetByName(currentPresetName);
			if (currentPreset) {
				ImGui::Separator();
				ImGui::Text("Lighting Settings");
				ImGui::ColorEdit3("Light Color", &currentPreset->lightColor.x);
				ImGui::SliderFloat("Light Intensity", &currentPreset->lightIntensity, 0.0f, 5.0f);
				ImGui::SliderFloat3("Light Direction", &currentPreset->lightDirection.x, -1.0f, 1.0f);
				ImGui::ColorEdit4("Clear Color", &currentPreset->clearColor.x);

				ImGui::Separator();
				ImGui::Text("Sky Settings");
				ImGui::ColorEdit4("Sky Color", &currentPreset->skyColor.x);
				ImGui::SliderFloat("Sky Brightness", &currentPreset->skyBrightness, 0.0f, 2.0f);

				ImGui::Separator();
				ImGui::Text("Particle Cloud Settings");
				ImGui::Checkbox("Enable Particle Clouds", &currentPreset->cloudEnabled);
				if (currentPreset->cloudEnabled) {
					ImGui::ColorEdit4("Cloud Color", &currentPreset->cloudColor.x);
					ImGui::SliderFloat("Cloud Density", &currentPreset->cloudDensity, 0.02f, 2.0f);
					ImGui::SliderFloat("Cloud Size", &currentPreset->cloudSize, 0.2f, 4.0f);
					ImGui::SliderFloat("Cloud Height Above Stage", &currentPreset->cloudAltitudeOffset, 2.0f, 30.0f);
				}

				ImGui::Separator();
				ImGui::Text("Particle Settings");
				ImGui::Checkbox("Enable Particle", &currentPreset->particleEnabled);
				if (currentPreset->particleEnabled) {
					// Texture is a string, we might just provide a predefined list or text input for now
					// ImGui::InputText("Texture", &currentPreset->particleTexture...); (skipped for simplicity, just use white.png)
					ImGui::SliderFloat("Emit Rate", &currentPreset->emitRate, 0.0f, 1000.0f);
					ImGui::SliderFloat3("Emit Size", &currentPreset->emitSize.x, 0.1f, 100.0f);
					ImGui::SliderFloat3("Velocity", &currentPreset->velocity.x, -20.0f, 20.0f);
					ImGui::SliderFloat3("Velocity Random", &currentPreset->velocityRandom.x, 0.0f, 10.0f);
					ImGui::SliderFloat3("Particle Size", &currentPreset->particleSize.x, 0.01f, 2.0f);
					ImGui::SliderFloat("LifeTime", &currentPreset->particleLife, 0.1f, 10.0f);
					ImGui::ColorEdit4("Particle Color", &currentPreset->particleColor.x);
					const char* impactNames[] = { "None", "Rain", "Snow" };
					int impactIndex = currentPreset->impactEffect == "Rain" ? 1
						: currentPreset->impactEffect == "Snow" ? 2 : 0;
					if (ImGui::Combo("Ground Impact", &impactIndex, impactNames, IM_ARRAYSIZE(impactNames))) {
						currentPreset->impactEffect = impactNames[impactIndex];
					}
				}

				if (!currentPreset->stormPreset.empty()) {
					ImGui::Text("Storm Effect: %s", currentPreset->stormPreset.c_str());
					ImGui::TextDisabled("Parameters come from the Effect Editor preset.");
				}

				if (ImGui::Button("Save Preset Changes")) {
					wpMgr.SavePresets();
				}
				ImGui::SameLine();
				if (ImGui::Button("Save as New Preset")) {
					WeatherPreset newPreset = *currentPreset;
					newPreset.name = currentPreset->name + " (Copy)";
					presets.push_back(newPreset);
					wpMgr.SavePresets();
					stageMap.SetWeatherPresetName(newPreset.name);
				}

				// 編集中の内容をリアルタイムでステージに反映する
				stageMap.SetClearColor(currentPreset->clearColor);
				stageMap.SetLightIntensity(currentPreset->lightIntensity);
				stageMap.SetLightColor(currentPreset->lightColor);
				stageMap.SetLightDirection(currentPreset->lightDirection);
			}
		}
	}

	// --- ステージの保存・読み込み管理 UI ---
	if (ImGui::CollapsingHeader("Stage Manager", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::InputText("Save Name", newStageName_, IM_ARRAYSIZE(newStageName_));
		// 新規保存ボタン
		if (ImGui::Button("Save As New")) {
			std::string path = "Resources/Stages/" + std::string(newStageName_) + ".txt";
			stageMap.SaveToFile(path);
			RefreshStageList();
		}

		ImGui::Separator();
		ImGui::Text("New Blank Stage");

		ImGui::InputInt("Width", &newStageWidth_);
		ImGui::InputInt("Height", &newStageHeight_);
		ImGui::InputInt("Depth", &newStageDepth_);

		// std::max は Windows の max マクロと衝突することがあるので使わない
		if (newStageWidth_ < 1) { newStageWidth_ = 1; }
		if (newStageHeight_ < 1) { newStageHeight_ = 1; }
		if (newStageDepth_ < 1) { newStageDepth_ = 1; }

		// 新しい空のステージを作成するボタン
		if (ImGui::Button("Create Blank Stage")) {
			stageMap.Initialize(newStageWidth_, newStageHeight_, newStageDepth_);

			// 新しいステージを作成したら、ステージレンダラーを更新して表示する
			if (stageRenderer) {
				stageRenderer->BuildFromStageMap(stageMap);
			}

			ResetPlayerToStartCell(stageMap, player);
		}

		// 保存されているステージファイルの一覧を表示
		ImGui::Text("Saved Stages:");
		// 保存されているステージファイルの一覧をリストボックス表示
		if (ImGui::BeginListBox("##StageList", ImVec2(-FLT_MIN, 100))) {
			for (int n = 0; n < (int)stageFiles_.size(); n++) {
				const bool is_selected = (selectedStageIndex_ == n);
				if (ImGui::Selectable(stageFiles_[n].c_str(), is_selected)) {
					selectedStageIndex_ = n;
				}
			}
			ImGui::EndListBox();
		}

		// リストから選択されているステージに対するアクション（ロード・上書き・削除）
		if (selectedStageIndex_ != -1 && selectedStageIndex_ < (int)stageFiles_.size()) {
			std::string fullPath = "Resources/Stages/" + stageFiles_[selectedStageIndex_] + ".txt";
			if (ImGui::Button("Load Selected")) {
				stageMap.LoadFromFile(fullPath);
				if (stageRenderer) {
					stageRenderer->BuildFromStageMap(stageMap);
				}
				ResetPlayerToStartCell(stageMap, player);
			}
			ImGui::SameLine();
			if (ImGui::Button("Overwrite")) {
				stageMap.SaveToFile(fullPath);
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
			if (ImGui::Button("Delete")) {
				std::filesystem::remove(fullPath);
				RefreshStageList();
				selectedStageIndex_ = -1;
			}
			ImGui::PopStyleColor();
		}
		// ステージ一覧を再スキャンするボタン
		if (ImGui::Button("Refresh List")) { RefreshStageList(); }
	}

	// --- 環境・ライティング設定 UI ---
	if (ImGui::CollapsingHeader("Environment & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector4 clearColor = stageMap.GetClearColor();
		if (ImGui::ColorEdit4("Sky/Clear Color", &clearColor.x)) {
			stageMap.SetClearColor(clearColor);
		}

		Vector3 lightColor = stageMap.GetLightColor();
		if (ImGui::ColorEdit3("Light Color", &lightColor.x)) {
			stageMap.SetLightColor(lightColor);
		}

		float lightIntensity = stageMap.GetLightIntensity();
		if (ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 3.0f)) {
			stageMap.SetLightIntensity(lightIntensity);
		}

		Vector3 lightDir = stageMap.GetLightDirection();
		bool dirChanged = false;
		dirChanged |= ImGui::SliderFloat("Light Dir X", &lightDir.x, -1.0f, 1.0f);
		dirChanged |= ImGui::SliderFloat("Light Dir Y", &lightDir.y, -1.0f, 1.0f);
		dirChanged |= ImGui::SliderFloat("Light Dir Z", &lightDir.z, -1.0f, 1.0f);
		if (dirChanged) {
			stageMap.SetLightDirection(lightDir);
		}
	}

	if (ImGui::CollapsingHeader("Stage Editor Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
		// ブロック全体の均等スケール調整用スライダー
		if (ImGui::SliderFloat("Uniform Block Scale", &editorUniformBlockScale_, 0.1f, 3.0f)) {
			editorBlockScale_ = { editorUniformBlockScale_, editorUniformBlockScale_, editorUniformBlockScale_ };
			if (stageRenderer) {
				stageRenderer->SetBlockScale(editorBlockScale_);
				stageRenderer->BuildFromStageMap(stageMap);
			}
		}
	}

	// --- 自分でブロックパーツを作成・カスタマイズする UI ---
	if (ImGui::CollapsingHeader("Custom Block Maker", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Select Custom Slot:");
		for (int i = 1; i <= 5; ++i) {
			char label[16];
			sprintf_s(label, "Part %d", i);
			// 5つのカスタムパーツスロットを横並びで表示
			if (i > 1) {
				ImGui::SameLine();
			}

			bool isCurrent = (selectedCustomPartSlot_ == i);
			// 選択中のスロットは色を変えて強調表示
			if (isCurrent) {
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.6f, 0.6f));
			}
			// スロットボタンを描画し、クリックされたら選択中のスロットを更新
			if (ImGui::Button(label)) {
				selectedCustomPartSlot_ = i;
			}
			// 選択中のスロットの色を元に戻す
			if (isCurrent) {
				ImGui::PopStyleColor();
			}
		}

		auto* part = stageMap.GetCustomPart(selectedCustomPartSlot_);
		// 選択中のカスタムパーツが存在する場合、編集UIを表示
		if (part) {
			bool changed = false;

			// 1. パーツ名編集
			char nameBuf[32];
			strcpy_s(nameBuf, part->name.c_str());
			if (ImGui::InputText("Part Name", nameBuf, sizeof(nameBuf))) {
				part->name = nameBuf;
			}

			// 2. カラー編集
			float color[3] = { part->colorR, part->colorG, part->colorB };
			if (ImGui::ColorEdit3("Color (RGB)", color)) {
				part->colorR = color[0];
				part->colorG = color[1];
				part->colorB = color[2];
				changed = true;
			}

			ImGui::Separator();
			ImGui::Text("--- 3x3x3 Shape Assembly Editor ---");
			ImGui::Text("Click cells to cycle: None -> Wall -> Ladder");

			// 編集対象のY座標（レイヤー 0〜2）
			static int editY = 0;
			ImGui::Text("Layer (Height Y):");
			for (int ly = 0; ly < 3; ++ly) {
				char layerLabel[16];
				sprintf_s(layerLabel, "Y = %d", ly);
				if (ly > 0) {
					ImGui::SameLine();
				}
				if (ImGui::RadioButton(layerLabel, &editY, ly)) {
					// レイヤー変更
				}
			}

			// 3x3 グリッドの描画 (z, x)
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
			for (int lz = 2; lz >= 0; --lz) { // 奥から手前へ
				for (int lx = 0; lx < 3; ++lx) {
					if (lx > 0) {
						ImGui::SameLine();
					}

					auto& cell = part->cells[editY][lz][lx];
					char btnLabel[64];

					// スロット番号とセル座標で一意なIDを作る
					sprintf_s(btnLabel, "%s##%d_%d_%d_%d",
						(cell.type == BlockType::Wall) ? "WALL" : (cell.type == BlockType::Ladder) ? "LAD" : " . ",
						selectedCustomPartSlot_, editY, lz, lx);

					// セル別のカラーをボタンカラーに反映
					if (cell.type != BlockType::None) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(part->colorR, part->colorG, part->colorB, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(part->colorR * 1.1f, part->colorG * 1.1f, part->colorB * 1.1f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(part->colorR * 0.8f, part->colorG * 0.8f, part->colorB * 0.8f, 1.0f));
					}
					else {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
					}

					// セルボタンを描画し、クリックされたらセルの種類をトグル切り替え
					if (ImGui::Button(btnLabel, ImVec2(55.0f, 40.0f))) {
						// トグル切り替え: None -> Wall -> Ladder -> None
						if (cell.type == BlockType::None) {
							cell.type = BlockType::Wall;
						}
						else if (cell.type == BlockType::Wall) {
							cell.type = BlockType::Ladder;
						}
						else {
							cell.type = BlockType::None;
						}
						changed = true;
					}
					ImGui::PopStyleColor(3);
				}
			}
			ImGui::PopStyleVar();

			// 一括クリアボタン
			if (ImGui::Button("Clear Entire Shape")) {
				for (int y = 0; y < 3; ++y) {
					for (int z = 0; z < 3; ++z) {
						for (int x = 0; x < 3; ++x) {
							part->cells[y][z][x].type = BlockType::None;
						}
					}
				}
				changed = true;
			}

			// 変更があったら3D表示をリアルタイム再構築！
			if (changed && stageRenderer) {
				stageRenderer->BuildFromStageMap(stageMap);
			}
		}
	}

	// --- ブロックタイプ別の追加設定 UI ---
	if (selectedBlockType_ == BlockType::BubblePickup)
	{
		// バブルの中身を選択するUI
		if (ImGui::CollapsingHeader("Bubble Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Bubble Contents:");

			int currentItem = 0;
			// bubbleInsideCustomSlot_ が 1〜5 の場合はカスタムパーツ、0 はデフォルトブロック
			if (bubbleInsideCustomSlot_ >= 1 && bubbleInsideCustomSlot_ <= 5) {
				currentItem = bubbleInsideCustomSlot_ + 1;
			}
			else {
				// デフォルトブロックの選択肢を反映
				currentItem = (bubbleInsideBlockType_ == BlockType::Ladder) ? 1 : 0;
			}

			// コンボボックスの選択肢を作成
			std::vector<std::string> comboItems = { "Default Wall", "Default Ladder" };
			for (int i = 1; i <= 5; ++i) {
				// カスタムパーツの名前を取得して表示名に反映
				const auto* part = stageMap.GetCustomPart(i);
				std::string displayName = "Custom " + std::to_string(i);
				// カスタムパーツの名前が空でなければ、表示名に追加
				if (part && !part->name.empty()) {
					displayName += " (" + part->name + ")";
				}
				comboItems.push_back(displayName);
			}

			std::vector<const char*> itemsPtr;
			// const char* の配列に変換して ImGui::Combo に渡す
			for (const auto& item : comboItems) {
				itemsPtr.push_back(item.c_str());
			}

			// コンボボックスを描画し、選択が変更された場合の処理
			if (ImGui::Combo("Inside Block", &currentItem, itemsPtr.data(), static_cast<int>(itemsPtr.size()))) {
				if (currentItem == 0) {
					bubbleInsideBlockType_ = BlockType::Wall;
					bubbleInsideCustomSlot_ = 0;
					// デフォルトブロックの選択肢を反映
				}
				else if (currentItem == 1) {
					bubbleInsideBlockType_ = BlockType::Ladder;
					bubbleInsideCustomSlot_ = 0;
				}
				else {
					// カスタムパーツの選択肢を反映
					bubbleInsideCustomSlot_ = currentItem - 1; // 1〜5
					const auto* part = stageMap.GetCustomPart(bubbleInsideCustomSlot_);
					if (part) {
						bubbleInsideBlockType_ = part->baseType;
					}
				}
			}
		}
	}

	// ドアの設定 UI
	if (selectedBlockType_ == BlockType::Door)
	{
		// ドアの設定を表示する折りたたみヘッダー
		if (ImGui::CollapsingHeader("Door Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			// ドアの番号を 1 〜 9 の間で選べるスライダー（または InputInt）
			ImGui::SliderInt("Door ID Number", &selectedDoorId_, 1, 9, "ID: %d");
			ImGui::Text("Doors with the same ID will connect to each other.");
		}
	}


	if (selectedBlockType_ == BlockType::PSwitch ||
		selectedBlockType_ == BlockType::PBlock ||
		selectedBlockType_ == BlockType::PBlockAppears)
	{
		// PスイッチとPブロックの設定を表示する折りたたみヘッダー
		if (ImGui::CollapsingHeader("P Switch Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderInt("P Switch ID Number", &selectedPSwitchId_, 1, 9, "ID: %d");
			ImGui::Text("PSwitch and PBlock with the same ID will connect.");
		}
	}


	if (selectedBlockType_ == BlockType::TimedBlock)
	{
		if (ImGui::CollapsingHeader("Timed Block Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderInt("Group ID", &selectedTimedGroupId_, 1, 9, "Group: %d");
			ImGui::SliderInt("Order ID", &selectedTimedOrderId_, 0, 9, "Order: %d");
			ImGui::Text("Blocks in the same group appear sequentially.");
		}
	}

	// ツールバー描画
	DrawEditorToolbar(stageMap, stageRenderer, mapCursor, player);

	if (ImGui::CollapsingHeader("Playlist Manager")) {
		ImGui::Text("Resources/Stages/sequence.txt");
		ImGui::Separator();

		ImGui::Columns(2, "PlaylistColumns");
		ImGui::Text("Available Stages");
		ImGui::NextColumn();
		ImGui::Text("Campaign Stages");
		ImGui::NextColumn();
		ImGui::Separator();

		ImGui::BeginChild("AvailableStages", ImVec2(0, 150), true);
		for (int i = 0; i < availableFiles_.size(); ++i) {
			if (ImGui::Selectable(availableFiles_[i].c_str(), selectedAvailableIndex_ == i)) {
				selectedAvailableIndex_ = i;
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("Add ->") && selectedAvailableIndex_ >= 0 && selectedAvailableIndex_ < availableFiles_.size()) {
			campaignFiles_.push_back(availableFiles_[selectedAvailableIndex_]);
			availableFiles_.erase(availableFiles_.begin() + selectedAvailableIndex_);
			selectedAvailableIndex_ = -1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Load##Available") && selectedAvailableIndex_ >= 0 && selectedAvailableIndex_ < availableFiles_.size()) {
			std::string fullPath = "Resources/Stages/" + availableFiles_[selectedAvailableIndex_];
			stageMap.LoadFromFile(fullPath);
			if (stageRenderer) { stageRenderer->BuildFromStageMap(stageMap); }
			ResetPlayerToStartCell(stageMap, player);
		}

		ImGui::NextColumn();

		ImGui::BeginChild("CampaignStages", ImVec2(0, 150), true);
		for (int i = 0; i < campaignFiles_.size(); ++i) {
			std::string label = "Stage " + std::to_string(i + 1) + ": " + campaignFiles_[i];
			if (ImGui::Selectable(label.c_str(), selectedCampaignIndex_ == i)) {
				selectedCampaignIndex_ = i;
			}
		}
		ImGui::EndChild();

		if (ImGui::Button("<- Remove") && selectedCampaignIndex_ >= 0 && selectedCampaignIndex_ < campaignFiles_.size()) {
			availableFiles_.push_back(campaignFiles_[selectedCampaignIndex_]);
			campaignFiles_.erase(campaignFiles_.begin() + selectedCampaignIndex_);
			selectedCampaignIndex_ = -1;
		}

		ImGui::SameLine();
		if (ImGui::Button("Load##Campaign") && selectedCampaignIndex_ >= 0 && selectedCampaignIndex_ < campaignFiles_.size()) {
			std::string fullPath = "Resources/Stages/" + campaignFiles_[selectedCampaignIndex_];
			stageMap.LoadFromFile(fullPath);
			if (stageRenderer) { stageRenderer->BuildFromStageMap(stageMap); }
			ResetPlayerToStartCell(stageMap, player);
		}

		ImGui::SameLine();
		if (ImGui::Button("Up") && selectedCampaignIndex_ > 0 && selectedCampaignIndex_ < campaignFiles_.size()) {
			std::swap(campaignFiles_[selectedCampaignIndex_], campaignFiles_[selectedCampaignIndex_ - 1]);
			selectedCampaignIndex_--;
		}

		ImGui::SameLine();
		if (ImGui::Button("Down") && selectedCampaignIndex_ >= 0 && selectedCampaignIndex_ < campaignFiles_.size() - 1) {
			std::swap(campaignFiles_[selectedCampaignIndex_], campaignFiles_[selectedCampaignIndex_ + 1]);
			selectedCampaignIndex_++;
		}

		ImGui::Columns(1);
		ImGui::Separator();

		if (ImGui::Button("Save Playlist")) {
			SaveCampaignSequence();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload Playlist")) {
			LoadCampaignSequence();
		}
	}


	ImGui::End(); // Stage Editor Window の End


#endif
}

// ImGui 内にブロック一覧ボタンや回転・配置・削除ボタンなどのツールバーを描画します。
void StageEditorController::DrawEditorToolbar(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
#ifdef USE_IMGUI
	if (!mapCursor) {
		return;
	}

	ImGui::Text("1. Select Type");
	ImGui::Separator();

	// カテゴリー定義
	struct Category
	{
		const char* name;
		std::vector<BlockType> types;
	};

	std::vector<Category> categories =
	{
		{
			"Basic Blocks", // ブロック類
			{
				BlockType::Ground,
				BlockType::Wall,
				BlockType::PBlock,
				BlockType::PBlockAppears,
				BlockType::CrumblingFloor,
				BlockType::IceBlock,
				BlockType::MovingFloor,
				BlockType::KeyBlock,
				BlockType::TimedBlock,
				BlockType::OnBlock,
				BlockType::OffBlock,
				BlockType::TransparentBlock
			}
		},
		{
			"Gimmicks & Interactables", // ギミック類
			{
				BlockType::Ladder,
				BlockType::Star,
				BlockType::BubblePickup,
				BlockType::Goal,
				BlockType::Door,
				BlockType::PSwitch,
				BlockType::Key,
				BlockType::OnOffSwitch
			}
		},
		{
			"Enemies", // 敵キャラクター
			{
				BlockType::EnemyWalker,
				BlockType::EnemyFlyer,
				BlockType::EnemyChaser
			}
		},
		{
			"System", //  その他
			{
				BlockType::PlayerStart,
				BlockType::Checkpoint
			}
		}
	};

	// タブバーを使ってカテゴリを分ける
	if (ImGui::BeginTabBar("BlockCategoryTabs")) {
		for (const auto& cat : categories) {
			if (ImGui::BeginTabItem(cat.name)) {

				float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
				for (int n = 0; n < cat.types.size(); n++) {
					BlockType type = cat.types[n];
					// 通常ブロックかつ bubbleInsideCustomSlot_ が 0 の場合のみ選択中とみなす
					bool isSelected = (selectedBlockType_ == type && bubbleInsideCustomSlot_ == 0);

					if (isSelected) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
					}

					if (ImGui::Button(BlockTypeToString(type), ImVec2(140, 30))) {
						selectedBlockType_ = type;
						bubbleInsideCustomSlot_ = 0; // 通常選択時はカスタムIDを解除
					}

					if (isSelected) {
						ImGui::PopStyleColor();
					}

					float last_button_x2 = ImGui::GetItemRectMax().x;
					float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 140;
					if (n + 1 < cat.types.size() && next_button_x2 < window_visible_x2) {
						ImGui::SameLine();
					}
				}
				ImGui::EndTabItem();
			}
		}

		// --- 新しいカテゴリタブ「Custom Blocks」を追加 ---
		if (ImGui::BeginTabItem("Custom Blocks")) {
			float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
			const auto& parts = stageMap.GetCustomParts();
			for (int i = 1; i <= 5; ++i) {
				const auto& part = parts[i - 1];

				// ボタンの選択状態：選択中ブロックタイプが part.baseType 且つ bubbleInsideCustomSlot_ == i
				bool isSelected = (selectedBlockType_ == part.baseType && bubbleInsideCustomSlot_ == i);

				if (isSelected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); // カスタムブロック選択は青色
				}

				// ボタンラベルはカスタムパーツの名前、空の場合は "Part i" とする
				std::string btnLabel = part.name;
				if (btnLabel.empty()) {
					btnLabel = "Part " + std::to_string(i);
				}

				// カスタムパーツボタンを描画し、クリックされたら選択中のブロックタイプとカスタムIDを更新
				if (ImGui::Button(btnLabel.c_str(), ImVec2(140, 30))) {
					selectedBlockType_ = part.baseType;
					bubbleInsideCustomSlot_ = i; // カスタムIDを適用
				}

				// 選択中のカスタムパーツの色を元に戻す
				if (isSelected) {
					ImGui::PopStyleColor();
				}

				// 横並びのレイアウト調整
				float last_button_x2 = ImGui::GetItemRectMax().x;
				float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 140;
				if (i < 5 && next_button_x2 < window_visible_x2) {
					ImGui::SameLine();
				}
			}
			ImGui::EndTabItem();
		}

		// ブロック選択UIの近くに追加
		if (selectedBlockType_ == BlockType::MovingFloor) {
			ImGui::Separator();
			ImGui::Text("Moving Floor Settings");
			// X, Y, Z の移動量を設定するスライダー（-10マス 〜 10マス の範囲など）
			ImGui::SliderInt("Move X", &currentMoveOffset_.x, -10, 10);
			ImGui::SliderInt("Move Y", &currentMoveOffset_.y, -10, 10);
			ImGui::SliderInt("Move Z", &currentMoveOffset_.z, -10, 10);
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("2. Action");
	// 回転ボタン（Rキーと同等）
	if (ImGui::Button("Rotate (R)", ImVec2(-FLT_MIN, 30))) {
		const Int3& cursor = mapCursor->GetIndex();
		MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
		if (cell && cell->type != BlockType::None) {
			cell->rotationY += 1.5708f;
			if (stageRenderer) {
				stageRenderer->BuildFromStageMap(stageMap);
			}
		}
	}

	ImGui::Spacing();

	// 配置実行ボタン（Enterキーと同等）
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
	if (ImGui::Button("PLACE (Enter)", ImVec2(-FLT_MIN, 40))) {
		ApplyPlacement(stageMap, stageRenderer, mapCursor, player);
	}
	ImGui::PopStyleColor();

	// 削除ボタン（Spaceキーと同等）
	if (ImGui::Button("REMOVE (Space)", ImVec2(-FLT_MIN, 40))) {
		stageMap.RemoveBlock(mapCursor->GetIndex());
		if (stageRenderer) {
			stageRenderer->BuildFromStageMap(stageMap);
		}
	}

#endif
}

// 現在のカーソル位置に対して、選択中のブロック（またはドアのペアリング）を配置・適用します。
void StageEditorController::ApplyPlacement(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
	if (!mapCursor) {
		return;
	}

	const Int3& cursor = mapCursor->GetIndex();
	MapCell* oldCell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);

	// ==========================================================
	// ドア配置時の特殊処理（ペアリング管理）
	// ==========================================================
	if (selectedBlockType_ == BlockType::Door)
	{
		// 第3引数の variant に選択中のドア番号 (selectedDoorId_) を渡して配置
		stageMap.SetBlock(cursor, BlockType::Door, selectedDoorId_);

		// 既存のファイル保存フォーマット（直後に座標を3つ要求する仕様）との互換性を保つため、
		// doorTargetIndex には一旦自身の座標かダミー値を書き込んでおきます
		MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
		if (cell) {
			cell->doorTargetIndex = cursor;
		}
	} // ==========================================================
	// ▼ 追加：Pスイッチ / Pブロック配置時の特殊処理
	// ==========================================================
	else if (selectedBlockType_ == BlockType::PSwitch ||
		selectedBlockType_ == BlockType::PBlock ||
		selectedBlockType_ == BlockType::PBlockAppears)
	{
		stageMap.SetBlock(cursor, selectedBlockType_, selectedPSwitchId_);

		MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
		// Pブロックの場合は、最初はすり抜ける状態にするかどうかを設定
		if (cell && selectedBlockType_ == BlockType::PBlock) {
			cell->isSolid = true;
			cell->isHidden = false;
		}
		else if (cell && selectedBlockType_ == BlockType::PBlockAppears) {
			cell->isSolid = false;  // 最初はすり抜ける状態
			cell->isHidden = false; // エディタで見えるようにする
		}
	}
	else if (selectedBlockType_ == BlockType::TimedBlock)
	{
		int variant = selectedTimedGroupId_ * 10 + selectedTimedOrderId_;
		stageMap.SetBlock(cursor, selectedBlockType_, variant);
	}
	else
	{
		// 通常のブロック配置
		int variant = 0;
		if (selectedBlockType_ == BlockType::BubblePickup) {
			// シャボン玉の場合：中身のベースタイプとカスタムIDをパックして variant に仕込む
			variant = PackBubbleContents(bubbleInsideBlockType_, bubbleInsideCustomSlot_);
			stageMap.SetBlock(cursor, selectedBlockType_, variant);
		}
		else if (selectedBlockType_ == BlockType::Wall || selectedBlockType_ == BlockType::Ladder) {
			// カスタムブロックを直接配置する場合：variant にカスタムIDをそのまま仕込む
			if (bubbleInsideCustomSlot_ >= 1 && bubbleInsideCustomSlot_ <= 5) {
				variant = bubbleInsideCustomSlot_;

				// 🌟 複合カスタムアセンブリパーツを一括配置！！！
				const auto* part = stageMap.GetCustomPart(bubbleInsideCustomSlot_);
				if (part && !part->IsEmpty()) {
					// アセンブリの各セルを一括配置
					for (int ly = 0; ly < 3; ++ly) {
						for (int lz = 0; lz < 3; ++lz) {
							for (int lx = 0; lx < 3; ++lx) {
								const auto& cell = part->cells[ly][lz][lx];
								if (cell.type == BlockType::None) {
									continue; // 空セルは無視
								}

								Int3 targetPos = { cursor.x + lx, cursor.y + ly, cursor.z + lz };
								if (stageMap.IsInside(targetPos)) {
									stageMap.SetBlock(targetPos, cell.type, bubbleInsideCustomSlot_);
								}
							}
						}
					}
				}
				else {
					// 空なら1マスだけフォールバック配置
					stageMap.SetBlock(cursor, selectedBlockType_, variant);
				}
			}
			else {
				// 通常の1マス配置
				stageMap.SetBlock(cursor, selectedBlockType_, variant);
			}
		}
		else {
			// その他の通常ブロック配置
			stageMap.SetBlock(cursor, selectedBlockType_, variant);
		}

		// プレイヤースタート地点の場合は即座にプレイヤー座標も更新する
		MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
		if (cell && selectedBlockType_ == BlockType::MovingFloor) {
			cell->moveOffset = currentMoveOffset_;
		}
		// プレイヤースタート地点の場合は即座にプレイヤー座標も更新する
		if (selectedBlockType_ == BlockType::PlayerStart && player) {
			Vector3 startPos = {
				static_cast<float>(cursor.x),
				static_cast<float>(cursor.y) + 1.1f,
				static_cast<float>(cursor.z)
			};

			player->SetPosition(startPos);
			player->SetRespawnPosition(startPos);
		}
	}

	// 見た目の再構築
	if (stageRenderer) {
		stageRenderer->BuildFromStageMap(stageMap);
	}
}

// キーの押下状態を判定し、押しっぱなし時の連続入力をサポートする関数
bool StageEditorController::RepeatKey(Input* input, BYTE key, int firstDelay, int interval) {
	if (input->TriggerKey(key)) {
		return true; // 押した瞬間
	}

	if (!input->PushKey(key)) {
		return false;
	}

	// 押しっぱなし中の連続入力
	if (holdFrame_ >= firstDelay && ((holdFrame_ - firstDelay) % interval == 0)) {
		return true;
	}

	return false;
}

// sequence.txt を読み込み、campaignFiles_ と availableFiles_ を更新する
void StageEditorController::LoadCampaignSequence() {
	campaignFiles_.clear();
	availableFiles_.clear();
	std::string sequencePath = "Resources/Stages/sequence.txt";
	std::string stageDir = "Resources/Stages/";

	// sequence.txt が存在する場合、各行を読み込んで campaignFiles_ に追加
	if (std::filesystem::exists(sequencePath)) {
		std::ifstream ifs(sequencePath);
		std::string line;
		while (std::getline(ifs, line)) {
			if (!line.empty()) {
				campaignFiles_.push_back(line);
			}
		}
	}

	// ステージディレクトリ内の .txt ファイルをスキャンして、sequence.txt に含まれていないものを availableFiles_ に追加
	if (std::filesystem::exists(stageDir)) {
		for (const auto& entry : std::filesystem::directory_iterator(stageDir)) {
			if (entry.is_regular_file()) {
				std::string fileName = entry.path().filename().string();
				if (fileName.ends_with(".txt") && fileName != "sequence.txt") {
					// Check if not in campaign
					if (std::find(campaignFiles_.begin(), campaignFiles_.end(), fileName) == campaignFiles_.end()) {
						availableFiles_.push_back(fileName);
					}
				}
			}
		}
	}
}

// sequence.txt に現在の campaignFiles_ の内容を書き込む
void StageEditorController::SaveCampaignSequence() {
	std::string sequencePath = "Resources/Stages/sequence.txt";
	std::ofstream ofs(sequencePath);
	for (const auto& fileName : campaignFiles_) {
		ofs << fileName << "\n";
	}
}
