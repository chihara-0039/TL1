#include "Player.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr float kPlayerModelForwardYawOffset = 3.14159265f;

std::string ToLower(std::string text) {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return text;
}

bool ContainsAnyNameHint(const std::string& text, const std::vector<std::string>& hints) {
	const std::string lowerText = ToLower(text);
	for (const auto& hint : hints) {
		if (lowerText.find(hint) != std::string::npos) {
			return true;
		}
	}
	return false;
}

int FindMotionIndexByName(const std::vector<MotionData>& motions, const std::vector<std::string>& hints) {
	for (size_t i = 0; i < motions.size(); ++i) {
		if (ContainsAnyNameHint(motions[i].name, hints)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}
}

Player::~Player() = default;

// 初期化：描画用コンポーネントとモデルを設定
void Player::Initialize(Object3dCommon* common, Model* model) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel(model);
	// キノピオ隊長のように、モデルを直立させるための初期回転
	object_->SetRotation({ 0.0f, 0.0f, 0.0f });
	
	// 自機キャラクターの微小光沢・高級メタル反射設定
	object_->SetShininess(0.6f);
	object_->SetMetallic(0.15f);
	object_->SetEmissive(0.0f);

	skinnedObject_.reset();
	isSkinned_ = false;
	animationState_ = AnimationState::Idle;
}

void Player::InitializeWithSkinnedGltf(Object3dCommon* common, DirectXCommon* dxCommon, const std::string& gltfPath, TextureManager* textureManager) {
	skinnedObject_ = std::make_unique<SkinnedObject>();
	skinnedObject_->InitializeFromGltf(common, dxCommon, gltfPath, textureManager);
	if (auto* model = skinnedObject_->GetModel()) {
		model->EnsureDefaultPlayerMotions();
	}
	// カスタムモーション再生モードにして、再生時間とインデックスをプログラム側で制御する
	skinnedObject_->SetPlayAnimation(false);
	skinnedObject_->SetPlayCustomAnimation(true);
	skinnedObject_->SetAnimationSpeed(1.0f);
	
	object_.reset();
	isSkinned_ = true;
	animationState_ = AnimationState::Idle;
}

void Player::InitializeWithDefaultSkinned(Object3dCommon* common, DirectXCommon* dxCommon, TextureManager* textureManager) {
	skinnedObject_ = std::make_unique<SkinnedObject>();
	skinnedObject_->Initialize(common, dxCommon, textureManager);
	if (auto* model = skinnedObject_->GetModel()) {
		model->EnsureDefaultPlayerMotions();
	}
	skinnedObject_->SetPlayAnimation(false);
	skinnedObject_->SetPlayCustomAnimation(true);
	skinnedObject_->SetAnimationSpeed(1.0f);
	
	object_.reset();
	isSkinned_ = true;
	animationState_ = AnimationState::Idle;
}

// 更新：移動・重力・当たり判定の処理
void Player::Update(const Input* input,StageMap& map, float cameraRotY, const Matrix4x4& lightVP, DirectXCommon* dxCommon)
{
	input_ = input;

	// UI表示フラグを毎フレームリセット
	isNearPSwitch_ = false;
	isNearKey_ = false;
	isNearKeyBlock_ = false;

	// --- 1. ハシゴ判定 ---
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gyWaist = static_cast<int>(std::floor(position_.y + 0.8f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	const MapCell* cellBottom = map.GetCell(gx, gyBottom, gz);
	const MapCell* cellWaist = map.GetCell(gx, gyWaist, gz);

	// 足元か腰がハシゴならハシゴモード
	bool isOnLadder = (cellBottom && cellBottom->type == BlockType::Ladder) ||
		(cellWaist && cellWaist->type == BlockType::Ladder);

	// はしごUI用フラグを毎フレーム更新
	isOnLadder_ = isOnLadder;

	if (isOnLadder_) {
		int ladderY = gyWaist;

		ladderWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(ladderY) + 1.2f,
			static_cast<float>(gz)
		};
	}

	Vector3 move = { 0, 0, 0 };
	bool hasMoveInput = false;
	bool hasRunInput = false;

	if (isOnLadder) 
	{

#pragma region はしご

		velocity_.y = 0; // ハシゴ中は重力による落下を止める

		// 入力方向の強さを計算（W/Sキーで上下、A/Dキーで左右に整理）
		float moveVertical = 0.0f;
		if (input->PushKey(DIK_W)) moveVertical += 1.0f;
		if (input->PushKey(DIK_S)) moveVertical -= 1.0f;
		moveVertical += input->GetGamePadState().leftStickY;
		moveVertical = std::clamp(moveVertical, -1.0f, 1.0f);

		float moveSide = 0.0f;
		if (input->PushKey(DIK_D)) moveSide += 1.0f;
		if (input->PushKey(DIK_A)) moveSide -= 1.0f;
		moveSide += input->GetGamePadState().leftStickX;
		moveSide = std::clamp(moveSide, -1.0f, 1.0f);
		hasMoveInput = std::abs(moveVertical) > 0.05f || std::abs(moveSide) > 0.05f;

		// 1. 登る・下りる入力がある時だけ上下移動と吸い寄せを行う
		if (moveVertical != 0.0f) {
			Vector3 nextPos = position_;
			nextPos.y += moveVertical * walkSpeed_;

			if (!CheckCollision(nextPos, map))
			{
				position_.y = nextPos.y;

				// ★修正：ハシゴの芯に吸い寄せる際にも壁判定を行う！
				// これで横からハシゴに触れてもブロックにめり込みません
				Vector3 targetPosX = position_;
				targetPosX.x += (static_cast<float>(gx) - position_.x) * 0.6f;
				if (!CheckCollision(targetPosX, map)) position_.x = targetPosX.x;

				Vector3 targetPosZ = position_;
				targetPosZ.z += (static_cast<float>(gz) - position_.z) * 0.6f;
				if (!CheckCollision(targetPosZ, map)) position_.z = targetPosZ.z;
			}
			else if (moveVertical > 0.0f)
			{
				// ★登りきり：ハシゴ自体の向きを使って押し出す
				float exitAngle = (cellWaist ? cellWaist->rotationY : rotation_.y);

				float pushForward = 0.2f;
				Vector3 exitPos = position_;
				exitPos.x += std::sin(exitAngle) * pushForward;
				exitPos.z += std::cos(exitAngle) * pushForward;
				// 少し上に上げて床判定を確実に踏ませる
				exitPos.y += 0.1f;

				// 押し出し先にも壁がないか一応チェックしてから移動
				if (!CheckCollision(exitPos, map)) {
					position_ = exitPos;
				}
			}
		}
				else if (moveSide != 0.0f)
		{
			// ハシゴ中の横移動もワールド座標基準で扱う。
			Vector3 nextPos = position_;
			nextPos.x += moveSide * walkSpeed_;
			
			if (!CheckCollision(nextPos, map)) {
				position_ = nextPos;
			}
		}

		// ハシゴ中は接地扱いにしてジャンプなどを可能にする
		isGrounded_ = true;

#pragma endregion

	} 
	else
	{
		// --- 【通常移動モード】（ここが消えていたのでフリーズしていました） ---

		// 入力方向をカメラの水平向きに合わせてワールド移動へ変換する。
		Vector3 inputDir = { 0, 0, 0 };
		if (input->PushKey(DIK_W)) inputDir.z += 1.0f;
		if (input->PushKey(DIK_S)) inputDir.z -= 1.0f;
		if (input->PushKey(DIK_A)) inputDir.x -= 1.0f;
		if (input->PushKey(DIK_D)) inputDir.x += 1.0f;
		inputDir.x += input->GetGamePadState().leftStickX;
		inputDir.z += input->GetGamePadState().leftStickY;

		if (inputDir.x != 0 || inputDir.z != 0) {
			float inputLength = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
			if (inputLength > 1.0f) {
				inputDir.x /= inputLength;
				inputDir.z /= inputLength;
			}
			hasRunInput = IsRunInputActive(inputDir);
			const float movementScale = hasRunInput ? 0.8f : 0.5f;
			inputDir.x *= movementScale;
			inputDir.z *= movementScale;
			hasMoveInput = true;

			move.x = inputDir.x * std::cos(cameraRotY) + inputDir.z * std::sin(cameraRotY);
			move.z = -inputDir.x * std::sin(cameraRotY) + inputDir.z * std::cos(cameraRotY);
			rotation_.y = std::atan2f(move.x, move.z) + kPlayerModelForwardYawOffset;
		}

#pragma region 滑る足場

		// 1. 足元のブロックを特定
		int gx = static_cast<int>(std::floor(position_.x + 0.5f));
		int gyBelow = static_cast<int>(std::floor(position_.y - 0.1f)); // 足の少し下
		int gz = static_cast<int>(std::floor(position_.z + 0.5f));

		const MapCell* cellBelow = map.GetCell(gx, gyBelow, gz);
		bool isOnIce = (cellBelow && cellBelow->type == BlockType::IceBlock);

		// 2. 加速度と摩擦係数を決定
		float acceleration = isOnIce ? 0.01f : 0.08f; // 氷なら加速が鈍い
		float friction = isOnIce ? 0.98f : 0.7f;     // 氷なら速度が減りにくい（1.0に近いほど滑る）

		// 加速
		velocity_.x += move.x * acceleration;
		velocity_.z += move.z * acceleration;

		// 摩擦（減速）
		velocity_.x *= friction;
		velocity_.z *= friction;

#pragma endregion

#pragma region 動く足場

		// ▼ ▼ 追加：動く足場への追従処理 ▼ ▼
		// 足元より少し下の位置をチェック
		Vector3 footCheckPos = position_;
		// プレイヤーの足元よりほんの少し低い位置を判定するための座標
		float footCheckY = position_.y - 0.05f;
		// 左右の判定半径を少しだけ小さく（0.8倍に）することで、ギリギリの端っこに乗ったときのガタつきを防ぐ
		float footRadiusX = radius_.x * 0.8f;
		float footRadiusZ = radius_.z * 0.8f;
		float footRadiusY = 0.05f; // 足元チェック用の薄い判定ボックスの高さ

		const MapCell* ridingFloor = map.GetIntersectingMovingFloor(position_.x, footCheckY, position_.z, footRadiusX, footRadiusY, footRadiusZ);
		if (ridingFloor) {
			// 足場が動いた分（deltaOffset）だけ、プレイヤーの座標も強制的に動かす
			position_.x += ridingFloor->deltaOffsetX;
			position_.y += ridingFloor->deltaOffsetY;
			position_.z += ridingFloor->deltaOffsetZ;

			// 足場の上に乗っているので、接地フラグを立てて重力による落下速度をリセット
			isGrounded_ = true;
			velocity_.y = 0.0f;
		}
		// ▲ ▲ ここまで ▲ ▲

#pragma endregion

#pragma region 中間地点

		// 🌟 追加：中間地点の判定
		// プレイヤーの現在位置のブロック座標を計算
		int px = static_cast<int>(std::floor(position_.x + 0.5f));
		int py = static_cast<int>(std::floor(position_.y + 0.5f)); // キャラクターの中心あたりの高さ
		int pz = static_cast<int>(std::floor(position_.z + 0.5f));

		MapCell* cell = map.GetCell(px, py, pz);
		if (cell && cell->type == BlockType::Checkpoint) {
			// 中間地点に触れたらリスポーン地点を更新する
			Vector3 checkpointPos = {
				static_cast<float>(px),
				static_cast<float>(py) + 1.1f, // 地面にめり込まないよう高さを調整
				static_cast<float>(pz)
			};
			SetRespawnPosition(checkpointPos);

			// ※必要に応じて「触れた瞬間に旗が上がる」ような演出を入れたり、
			// 何度も判定されないように cell->variant = 1; のようにして
			// 状態をフラグ管理するとより本格的になります！
		}

#pragma endregion

		// ジャンプ
		if (isGrounded_ && IsJumpTriggered()) {
			velocity_.y = 0.2f;
			isGrounded_ = false;
		}

		// 重力適用
		velocity_.y += gravity_;

		// X軸衝突判定
		Vector3 nextPosX = position_;
		nextPosX.x += velocity_.x;
		if (!CheckCollision(nextPosX, map)) position_.x = nextPosX.x;

		// Z軸衝突判定
		Vector3 nextPosZ = position_;
		nextPosZ.z += velocity_.z;
		if (!CheckCollision(nextPosZ, map)) position_.z = nextPosZ.z;

		// Y軸衝突判定
		Vector3 nextPosY = position_;
		nextPosY.y += velocity_.y;
		if (CheckCollision(nextPosY, map)) {
			if (velocity_.y < 0) isGrounded_ = true;
			velocity_.y = 0;
		} else {
			position_.y = nextPosY.y;
			isGrounded_ = false;
		}
	}

	CrumbleUpdate(map);
	PSwitchUpdate(map);
	DoorWarp(map);
	OnOffSwitchUpdate(map);
	
	// ▼ 追加：鍵の取得チェック
	KeyUpdate(map);

	//鍵を持った状態で触れているかチェック
	KeyBlockUIUpdate(map);

	// --- 敵やトゲとの接触判定 ---
	{
		int px = static_cast<int>(std::floor(position_.x + 0.5f));
		int py = static_cast<int>(std::floor(position_.y + 0.5f));
		int pz = static_cast<int>(std::floor(position_.z + 0.5f));

		float pMinX = position_.x - radius_.x;
		float pMaxX = position_.x + radius_.x;
		float pMinY = position_.y;
		float pMaxY = position_.y + radius_.y * 2.0f;
		float pMinZ = position_.z - radius_.z;
		float pMaxZ = position_.z + radius_.z;

		// ① トゲとの接触判定（静的ブロックのためプレイヤーの周囲のみを高速探索）
		for (int dy = -1; dy <= 2; ++dy) {
			for (int dz = -1; dz <= 1; ++dz) {
				for (int dx = -1; dx <= 1; ++dx) {
					int cx = px + dx;
					int cy = py + dy;
					int cz = pz + dz;

					const MapCell* cell = map.GetCell(cx, cy, cz);
					if (cell && !cell->isHidden) {
						if (cell->type == BlockType::Spike) 
						{
							float eX = static_cast<float>(cx) + cell->currentOffsetX;
							float eY = static_cast<float>(cy) + cell->currentOffsetY;
							float eZ = static_cast<float>(cz) + cell->currentOffsetZ;

							float eMinX = eX - 0.4f;
							float eMaxX = eX + 0.4f;
							float eMinY = eY - 0.5f;
							float eMaxY = eY - 0.2f; // トゲ床は低い
							float eMinZ = eZ - 0.4f;
							float eMaxZ = eZ + 0.4f;

							if (pMinX <= eMaxX && pMaxX >= eMinX &&
								pMinY <= eMaxY && pMaxY >= eMinY &&
								pMinZ <= eMaxZ && pMaxZ >= eMinZ) 
							{
								Respawn();
								goto collision_end;
							}
						}
					}
				}
			}
		}

		// ② 敵キャラクターとの接触判定（動いている敵の現在座標との正確な判定）
		for (const auto& enemyRef : map.GetEnemies()) {
			const MapCell* cell = map.GetCell(enemyRef.x, enemyRef.y, enemyRef.z);
			if (cell && !cell->isHidden) {
				float eX = static_cast<float>(enemyRef.x) + cell->currentOffsetX;
				float eY = static_cast<float>(enemyRef.y) + cell->currentOffsetY;
				float eZ = static_cast<float>(enemyRef.z) + cell->currentOffsetZ;

				float eMinX = eX - 0.4f;
				float eMaxX = eX + 0.4f;
				float eMinY = eY - 0.4f;
				float eMaxY = eY + 0.4f;
				float eMinZ = eZ - 0.4f;
				float eMaxZ = eZ + 0.4f;

				if (pMinX <= eMaxX && pMaxX >= eMinX &&
					pMinY <= eMaxY && pMaxY >= eMinY &&
					pMinZ <= eMaxZ && pMaxZ >= eMinZ) 
				{
					Respawn();
					goto collision_end;
				}
			}
		}
	}
collision_end:

	// --- 表示更新 ---
	if (isSkinned_ && skinnedObject_) {
		skinnedObject_->SetPosition(position_);
		skinnedObject_->SetRotation(rotation_);
		skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });

		bool isMoving = hasMoveInput || std::abs(velocity_.x) > 0.01f || std::abs(velocity_.z) > 0.01f;
		ApplySkinnedAnimation(ResolveAnimationState(isMoving, hasRunInput), isMoving);

		skinnedObject_->Update(dxCommon, lightVP);
	} else if (object_) {
		object_->SetPosition(position_);
		object_->SetRotation(rotation_);
		object_->Update(lightVP);
	}
}

// Object3d の行列を更新する（ライトカメラの行列も渡す）
void Player::UpdateTransform(const Matrix4x4& lightVP) 
{
	if (isSkinned_ && skinnedObject_) {
		skinnedObject_->SetPosition(position_);
		skinnedObject_->SetRotation(rotation_);
		skinnedObject_->Update(nullptr, lightVP);
	} else if (object_) {
		object_->Update(lightVP);
	}
}

// ドアに触れているか判定して、触れていてかつFキーがトリガーされたらワープする
void Player::DoorWarp(const StageMap& map)
{
	// 1. プレイヤーの足元のグリッド座標（整数）を計算
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	// 現在足元にあるセルを取得
	const MapCell* cell = map.GetCell(gx, gyBottom, gz);

	// 毎フレーム一度falseに戻す
	isNearDoor_ = false;

	// 足元がドアブロックだった場合
	if (cell && cell->type == BlockType::Door)
	{
		isNearDoor_ = true;

		// ドアの上に「Fキー」などのUIを出すワールド座標を設定
		nearDoorWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(gyBottom) + 1.0f,
			static_cast<float>(gz)
		};

		// 【ワープ実行処理】
		// ドアの中にいて、Fキーが押され、かつ「ワープ直後フラグ」が立っていない場合のみ実行
		if (IsInteractTriggered() && !hasJustWarped_)
		{
			// 2. マップ全体から、自分（gx, gyBottom, gz）と同じドア番号(variant)を持つ相方の座標を検索
			// ※前回 StageMap に追加した関数を呼び出します
			Int3 destination = map.FindPairedDoor(gx, gyBottom, gz);

			// 3. 相方のドアが見つかった場合（検索結果が現在の座標と異なる場合）
			if (destination.x != gx || destination.y != gyBottom || destination.z != gz)
			{
				// ワープ先の座標を設定
				// ※ destination.y（ブロックの底面）にプレイヤーの足元がぴったり乗るよう、
				//    キノピオ隊長の着地位置として少しだけ高さを浮かせます（+0.1fなど環境に合わせて調整）
				position_.x = static_cast<float>(destination.x);
				position_.y = static_cast<float>(destination.y) + 0.1f;
				position_.z = static_cast<float>(destination.z);

				// ワープした衝撃で物理移動がバグらないよう、落下速度や移動慣性を完全にゼロにリセット
				velocity_ = { 0.0f, 0.0f, 0.0f };

				// ワープ直後フラグを立てる（これでこのフレームや次フレームでの連続誤作動を防ぐ）
				hasJustWarped_ = true;
			}
		}
	}
	else
	{
		// 4. ドアから完全に離れたら、再ワープ防止フラグをリセットする
		// これにより、別のドア（または一度離れて入り直した時）で再びワープができるようになります
		hasJustWarped_ = false;
	}
}

void Player::KeyUpdate(StageMap& map)
{
	// プレイヤーの中心付近（足元〜腰）のマスを取得
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gy = static_cast<int>(std::floor(position_.y + 0.5f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	MapCell* cell = map.GetCell(gx, gy, gz);

	if (cell && cell->type == BlockType::Key) {
		isNearKey_ = true;
		nearKeyWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(gy) + 1.0f,
			static_cast<float>(gz)
		};

		if (IsInteractTriggered()) {
			hasKey_ = true;
			cell->type = BlockType::None;
			cell->isSolid = false;
			map.RequestRebuild();
		}
	}
	
}

//鍵ブロックUI判定関数

void Player::KeyBlockUIUpdate(StageMap& map)
{
	if (!hasKey_) {
		return;
	}

	int px = static_cast<int>(std::floor(position_.x + 0.5f));
	int py = static_cast<int>(std::floor(position_.y + 0.5f));
	int pz = static_cast<int>(std::floor(position_.z + 0.5f));

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dz = -1; dz <= 1; ++dz) {
				const MapCell* cell = map.GetCell(px + dx, py + dy, pz + dz);

				if (cell && cell->type == BlockType::KeyBlock) {
					isNearKeyBlock_ = true;
					nearKeyBlockWorldPos_ = {
						static_cast<float>(px + dx),
						static_cast<float>(py + dy) + 1.0f,
						static_cast<float>(pz + dz)
					};
					return;
				}
			}
		}
	}
}

bool Player::IsJumpTriggered() const {
	return input_ &&
		(input_->TriggerKey(DIK_SPACE) ||
		 input_->TriggerControllerButton(XINPUT_GAMEPAD_A));
}

bool Player::IsInteractTriggered() const {
	return input_ &&
		(input_->TriggerKey(DIK_F) ||
		 input_->TriggerControllerButton(XINPUT_GAMEPAD_X));
}

bool Player::IsRunInputActive(const Vector3& inputDir) const {
	if (!input_) {
		return false;
	}

	if (input_->PushKey(DIK_LSHIFT) || input_->PushKey(DIK_RSHIFT)) {
		return true;
	}

	const float stickPower = std::sqrt(inputDir.x * inputDir.x + inputDir.z * inputDir.z);
	return input_->IsGamePadConnected() && stickPower > 0.85f;
}

Player::AnimationState Player::ResolveAnimationState(bool hasMoveInput, bool isRunInput) const {
	if (isOnLadder_) {
		return AnimationState::Ladder;
	}
	if (!isGrounded_) {
		return AnimationState::Jump;
	}
	if (hasMoveInput) {
		return isRunInput ? AnimationState::Run : AnimationState::Walk;
	}
	return AnimationState::Idle;
}

void Player::ApplySkinnedAnimation(AnimationState state, bool isMoving) {
	if (!skinnedObject_) {
		return;
	}

	auto* model = skinnedObject_->GetModel();
	if (!model) {
		return;
	}

	const auto& motions = model->GetMotions();
	if (motions.empty()) {
		return;
	}

	int targetMotionIndex = 0;
	switch (state) {
	case AnimationState::Idle:
		targetMotionIndex = FindMotionIndexByName(motions, { "idle", "wait", "stand" });
		if (targetMotionIndex < 0) targetMotionIndex = 0;
		break;
	case AnimationState::Walk:
		targetMotionIndex = FindMotionIndexByName(motions, { "walk" });
		if (targetMotionIndex < 0) targetMotionIndex = motions.size() >= 2 ? 1 : 0;
		break;
	case AnimationState::Run:
		targetMotionIndex = FindMotionIndexByName(motions, { "run", "sprint" });
		if (targetMotionIndex < 0) {
			targetMotionIndex = FindMotionIndexByName(motions, { "walk" });
		}
		if (targetMotionIndex < 0) targetMotionIndex = motions.size() >= 2 ? 1 : 0;
		break;
	case AnimationState::Jump:
		targetMotionIndex = FindMotionIndexByName(motions, { "jump", "air", "fall" });
		if (targetMotionIndex < 0) targetMotionIndex = motions.size() >= 3 ? 2 : 0;
		break;
	case AnimationState::Ladder:
		targetMotionIndex = FindMotionIndexByName(motions, { "ladder", "climb" });
		if (targetMotionIndex < 0) targetMotionIndex = motions.size() >= 4 ? 3 : 0;
		break;
	}

	model->SetActiveMotionIndex(targetMotionIndex);

	const bool stateChanged = animationState_ != state;
	animationState_ = state;

	if (motions.size() == 1 && state == AnimationState::Idle && !isMoving) {
		skinnedObject_->SetPlayCustomAnimation(false);
		skinnedObject_->ApplyMotion(0.0f);
		return;
	}

	if (stateChanged) {
		skinnedObject_->SetCurrentKeyframeTime(0.0f);
	}

	skinnedObject_->SetAnimationSpeed(state == AnimationState::Run ? 1.4f : 1.0f);
	skinnedObject_->SetPlayCustomAnimation(true);
}

// 衝突判定ロジック
bool Player::CheckCollision(const Vector3& pos, StageMap& map) {
	// プレイヤーの当たり判定ボックス（四隅など）が StageMap の solid なセルに重なっているか
	// 足元、腰、頭の3段階で高さをチェック
	float checkOffsetsY[] = { 0.1f, 0.8f, 1.5f };

	for (float dy : checkOffsetsY) {
		for (float dx : { -radius_.x, radius_.x }) {
			for (float dz : { -radius_.z, radius_.z }) {
				// ワールド座標からマップのインデックス（整数）に変換
				int gx = static_cast<int>(std::floor(pos.x + dx + 0.5f));
				int gy = static_cast<int>(std::floor(pos.y + dy));
				int gz = static_cast<int>(std::floor(pos.z + dz + 0.5f));

				// 指定した座標のセル情報を取得
				MapCell* cell = map.GetCell(gx, gy, gz);

				// ▼ 追加：鍵ブロックの判定と破壊 ▼
				if (cell && cell->type == BlockType::KeyBlock) {
					if (hasKey_) {
						// 鍵を持っている場合は開ける（消費する）
						hasKey_ = false;
						// ★ 変更：1マスだけではなく、繋がっている塊をすべて消す
						map.RemoveConnectedKeyBlocks(gx, gy, gz);
						map.RequestRebuild();         // ステージの見た目を再構築

						// ブロックが消えたので、ここには壁が無いこととして判定を続ける
						continue;
					}
					else {
						// 鍵を持っていない場合は普通の壁として扱う
						return true;
					}
				}
				// ▲ ここまで ▲

				// ★ 変更：動く足場は固定グリッド判定から除外する
				if (cell && cell->isSolid && cell->type != BlockType::MovingFloor) {
					return true;
				}

				// 秋元追加 04/03
				if (cell && cell->type == BlockType::PBlock) {
					// PスイッチがONの時だけ「壁」として扱う
					if (map.IsPSwitchActive()) {
						return false;
					}
					return true; // OFFの時は通り抜けられる
				}
			}
		}
	}

	// ★ 追加：動いている足場のワールド座標判定（floatにバラして渡す）
	if (map.GetIntersectingMovingFloor(pos.x, pos.y, pos.z, radius_.x, radius_.y, radius_.z)) {
		return true;
	}

	if (CheckExternalCollision(pos)) {
		return true;
	}

	return false;
}

bool Player::CheckExternalCollision(const Vector3& pos) const {
	// 外部レベルを使わない従来ステージでは、追加判定を行わず即座に終了する。
	if (!externalCollisionBoxes_) {
		return false;
	}

	// position_は足元基準なので、Y方向だけ上へプレイヤー身長を伸ばす。
	const Vector3 playerMin = {
		pos.x - radius_.x,
		pos.y,
		pos.z - radius_.z
	};
	const Vector3 playerMax = {
		pos.x + radius_.x,
		pos.y + radius_.y * 2.0f,
		pos.z + radius_.z
	};

	// 辺同士が触れているだけの状態は「重なり」に含めない。
	// これにより床の上へ静止しているプレイヤーが常時衝突状態になるのを防ぐ。
	for (const WorldCollisionBox& box : *externalCollisionBoxes_) {
		if (playerMin.x < box.maximum.x && playerMax.x > box.minimum.x &&
			playerMin.y < box.maximum.y && playerMax.y > box.minimum.y &&
			playerMin.z < box.maximum.z && playerMax.z > box.minimum.z) {
			return true;
		}
	}
	return false;
}

// リスポーン処理：座標をリスポーンポイントに戻し、速度と回転をリセット
void Player::Respawn()
{
	position_ = respawnPosition_;
	velocity_ = { 0.0f,0.0f,0.0f };
	rotation_ = { 0.0f,0.0f,0.0f };

	// ▼ 追加：リスポーン時は鍵を失う
	hasKey_ = false;
}

// Pスイッチの更新：足元のセルをチェックして、Pスイッチがあればマップに状態変更を通知
void Player::PSwitchUpdate(StageMap& map)
{
	// プレイヤーの中心座標から足元のインデックスを計算
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	// 0.1fだと浮いている判定になりやすいので、少し余裕を持たせるか
	// 現在の座標(position_.y)の真下を正確に狙います
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	const MapCell* cellBelow = map.GetCell(gx, gyBottom, gz);

	if (cellBelow && cellBelow->type == BlockType::PSwitch) {
		isNearPSwitch_ = true;
		nearPSwitchWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(gyBottom) + 1.0f,
			static_cast<float>(gz)
		};

		if (IsInteractTriggered()) {
			map.SetPSwitchActive(cellBelow->variant);
		}
	}
}

void Player::OnOffSwitchUpdate(StageMap& map)
{
	if (IsInteractTriggered()) {
		// プレイヤーの現在位置（マス目）
		int px = static_cast<int>(std::floor(position_.x + 0.5f));
		int py = static_cast<int>(std::floor(position_.y + 0.5f));
		int pz = static_cast<int>(std::floor(position_.z + 0.5f));

		bool switchFound = false;

		// プレイヤーの周囲（上下左右前後 3x3x3マス）をチェック
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				for (int dz = -1; dz <= 1; ++dz) {
					const MapCell* cell = map.GetCell(px + dx, py + dy, pz + dz);
					// 近くにスイッチがあればフラグを立てる
					if (cell && cell->type == BlockType::OnOffSwitch) {
						switchFound = true;
					}
				}
			}
		}

		// スイッチが見つかっていればON/OFFを切り替える
		if (switchFound) {
			map.ToggleOnState();
			// （効果音を鳴らす処理をここに入れると気持ちいいです！）
		}
	}
}

// 描画：内部で持っている Object3d を描画
void Player::Draw() {
	if (isSkinned_ && skinnedObject_) {
		skinnedObject_->Draw();
	} else if (object_) {
		object_->Draw();
	}
}

// 影の描画：ライトカメラの行列を渡して影を描く
void Player::DrawShadow(const Matrix4x4& lightViewProjection) {
	if (isSkinned_ && skinnedObject_) {
		skinnedObject_->DrawShadow(lightViewProjection);
	} else if (object_) {
		object_->DrawShadow(lightViewProjection);
	}
}

void Player::DrawHighlight() {
	if (isSkinned_ && skinnedObject_) {
		auto* obj3d = skinnedObject_->GetObject3d();
		if (!obj3d) return;

		// 1回目：一番外側の大きい白
		obj3d->SetScale({ 1.55f, 1.55f, 1.55f });
		obj3d->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		obj3d->SetEnableLighting(false);
		obj3d->Draw();

		// 2回目：中間の白
		obj3d->SetScale({ 1.35f, 1.35f, 1.35f });
		obj3d->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		obj3d->SetEnableLighting(false);
		obj3d->Draw();

		// 3回目：本体に近い白
		obj3d->SetScale({ 1.18f, 1.18f, 1.18f });
		obj3d->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		obj3d->SetEnableLighting(false);
		obj3d->Draw();

		// 元に戻す
		obj3d->SetScale({ 1.0f, 1.0f, 1.0f });
		obj3d->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		obj3d->SetEnableLighting(true);
	} else if (object_) {
		// 1回目：一番外側の大きい白
		object_->SetScale({ 1.55f, 1.55f, 1.55f });
		object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		object_->SetEnableLighting(false);
		object_->Draw();

		// 2回目：中間の白
		object_->SetScale({ 1.35f, 1.35f, 1.35f });
		object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		object_->SetEnableLighting(false);
		object_->Draw();

		// 3回目：本体に近い白
		object_->SetScale({ 1.18f, 1.18f, 1.18f });
		object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		object_->SetEnableLighting(false);
		object_->Draw();

		// 元に戻す
		object_->SetScale({ 1.0f, 1.0f, 1.0f });
		object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		object_->SetEnableLighting(true);
	}
}

void Player::CrumbleUpdate(StageMap& map) {
	int gx = static_cast<int>(std::floor(position_.x ));
	int gyBottom = static_cast<int>(std::floor(position_.y - 0.05f));
	int gz = static_cast<int>(std::floor(position_.z));

	MapCell* cellBelow = map.GetCell(gx, gyBottom, gz);

	if (cellBelow && cellBelow->type == BlockType::CrumblingFloor && !cellBelow->isHidden) {
		cellBelow->isCrumbling = true;
	}
}
