#include "StageRenderer.h"
#include "StageWallTransparencyController.h"
#include "StagePlacementPreviewBuilder.h"
#include "StageOnOffVisualController.h"
#include "StageCrumblingFloorEffectUpdater.h"
#include <cassert>
#include <random>


// StageRenderer が所有している描画オブジェクトをまとめて解放する。
StageRenderer::~StageRenderer() {
	Clear();
}

// ステージ描画で使うモデルと、インスタンシング用の共通バッファを初期化する。
void StageRenderer::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon);
	object3dCommon_ = object3dCommon;

	// ブロック種別ごとに表示モデルを読み込む。StageMap のセル種別とここでのモデルが対応する。
	groundModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/block",
		"block.obj",
		object3dCommon_->GetTextureManager()
	);

	// 壁モデル。通常壁、Pブロック、ON/OFFブロックなど複数用途で使い回す。
	wallModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// はしごモデル設定
	ladderModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/ladder",
		"ladder.obj",
		object3dCommon_->GetTextureManager()
	);

	// シャボン玉モデル設定
	bubbleModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/soapBubbles",
		"soapBubbles.obj",
		object3dCommon_->GetTextureManager()
	);


	// ゴールモデル設定
	goalModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/star",
		"star.obj",
		object3dCommon_->GetTextureManager()
	);

	// ドアモデル設定
	doorModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/door",
		"door.obj",
		object3dCommon_->GetTextureManager()
	);

	// Pスイッチモデル設定
	pSwichModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/switch",
		"switch.obj",
		object3dCommon_->GetTextureManager()
	);

	// Pブロックモデル設定
	pBlockOnModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// 崩れる足場
	crumbleModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/CollapsedBlocks",
		"CollapsedBlocks.obj",
		object3dCommon_->GetTextureManager()
	);
	// 滑る足場
	iceBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/iceBlock",
		"iceBlock.obj",
		object3dCommon_->GetTextureManager()
	);
	// 動く足場
	movingFloorModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);
	// ▼ 追加：鍵モデル設定
	keyModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/key",
		"key.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：鍵ブロックモデル設定
	keyBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：中間地点モデル設定
	checkpointModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/midpoint",
		"midpoint.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：トゲモデル設定
	spikeModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/spike",
		"spike.obj",
		object3dCommon_->GetTextureManager()
	);

	// ONブロックモデル
	onBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	//OFFブロックモデル
	offBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// ONOFFスイッチモデル
	onOffSwichModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/switch",
		"switch.obj",
		object3dCommon_->GetTextureManager()
	);

	// インスタンシング描画時に全インスタンスで共有する ViewProjection 定数バッファを作成する。
	D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = (sizeof(ViewProjectionMatrix) + 0xff) & ~0xff;
	resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

	HRESULT hr = object3dCommon_->GetDxCommon()->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&viewProjectionResource_)
	);
	assert(SUCCEEDED(hr));
	viewProjectionResource_->Map(0, nullptr, (void**)&viewProjectionData_);
}

void StageRenderer::UpdateEffect(const StageMap& stageMap) {
	// 崩れる床など、セル状態に応じて見た目だけが変わるオブジェクトを更新する。
	for (Object3d* obj : StageCrumblingFloorEffectUpdater::Apply(stageMap, isEditorMode_, objects_)) {
		MarkDirty(obj);
	}
}

// StageMap の全セルを読み取り、描画用 Object3d を再構築する。
// ステージ編集やロード後など、ブロック構成が大きく変わった時に呼ぶ。
void StageRenderer::BuildFromStageMap(const StageMap& stageMap) {
	// 既存のオブジェクトがあれば全て削除してから新しいオブジェクトを生成する
	Clear();

	// ステージ寸法を元に、背景装飾の雲をステージ周辺へ配置する。
	int mapWidth = stageMap.GetWidth();
	int mapHeight = stageMap.GetHeight();
	int mapDepth = stageMap.GetDepth();
	float scaleX = blockScale_.x;
	float scaleY = blockScale_.y;
	float scaleZ = blockScale_.z;

	// 雲の生成を決定論的（再現可能）にするために固定シードの乱数生成器を使用する
	std::mt19937 randomEngine(12345);
	std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
	auto randomFloat = [&randomEngine, &dist01]() {
		return dist01(randomEngine);
	};

	// 背景雲は天候と色・密度を共有できるParticleManager側で生成する。
	// 旧来の球体モデル雲は二重表示を避けるため生成しない。
	int cloudCount = 0;
	for (int cloudIndex = 0; cloudIndex < cloudCount; ++cloudIndex) {
		CloudInstance cloud;
		// ランダムな位置 (ステージの少し上空、周囲)
		float randomCloudX = randomFloat() * (mapWidth * scaleX + 80.0f) - 40.0f;
		float randomCloudY = randomFloat() * 6.0f + 3.0f; // 3.0f 〜 9.0f の高さ
		float randomCloudZ = randomFloat() * (mapDepth * scaleZ + 80.0f) - 40.0f;
		cloud.basePosition = { randomCloudX, randomCloudY, randomCloudZ };

		// 流れる速度 (X軸方向へゆっくり流れる)
		float speedX = randomFloat() * 0.4f + 0.1f;
		cloud.speed = { speedX, 0.0f, 0.0f };

		// フワフワパラメータ
		cloud.floatTimer = randomFloat() * 6.28f;
		cloud.floatSpeed = randomFloat() * 0.3f + 0.1f;

		// 1つの雲を構成する球体数 (3〜5個)
		int partCount = static_cast<int>(randomEngine() % 3) + 3;
		for (int partIndex = 0; partIndex < partCount; ++partIndex) {
			// 中心からのオフセット
			float localOffsetX = randomFloat() * 4.0f - 2.0f;
			float localOffsetY = randomFloat() * 1.5f - 0.75f;
			float localOffsetZ = randomFloat() * 4.0f - 2.0f;
			cloud.localOffsets.push_back({ localOffsetX, localOffsetY, localOffsetZ });

			// ランダムスケール
			float s = randomFloat() * 2.0f + 1.5f;
			cloud.localScales.push_back({ s, s * 0.5f, s }); // 雲らしく少し平べったくする
		}

		// 3Dオブジェクトの作成
		for (int partIndex = 0; partIndex < partCount; ++partIndex) {
			std::unique_ptr<Object3d> cloudPartObject = std::make_unique<Object3d>();
			cloudPartObject->Initialize(object3dCommon_);
			cloudPartObject->SetModel(bubbleModel_.get()); // 球体モデルを雲のパーツとして使用
			cloudPartObject->SetEnableLighting(true);       // ライティングで立体感（ローポリ雲の綺麗な陰影）を出す
			cloudPartObject->SetShininess(0.0f);            // テカらせない
			cloudPartObject->SetMetallic(0.0f);             // テカらせない
			
			// 初期位置とスケール
			Vector3 cloudPartPosition = {
				cloud.basePosition.x + cloud.localOffsets[partIndex].x,
				cloud.basePosition.y + cloud.localOffsets[partIndex].y,
				cloud.basePosition.z + cloud.localOffsets[partIndex].z
			};
			cloudPartObject->SetPosition(cloudPartPosition);
			cloudPartObject->SetScale(cloud.localScales[partIndex]);
			
			// 色を完全な白に設定
			cloudPartObject->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			cloud.objects.push_back(std::move(cloudPartObject));
		}

		clouds_.push_back(std::move(cloud));
	}


	// ステージマップの全セルを走査して、ブロックがある場所に対応するモデルのオブジェクトを生成していく
	for (int y = 0; y < stageMap.GetHeight(); y++) {
		// 深さ方向もループして、全てのセルをチェック
		for (int z = 0; z < stageMap.GetDepth(); z++) {
			// 横方向のループ
			for (int x = 0; x < stageMap.GetWidth(); x++) {
				// ステージマップからセルの情報を取得
				const MapCell* cell = stageMap.GetCell(x, y, z);
				// セルが存在しない（範囲外）場合はスキップ
				if (!cell) {
					// 範囲外のセルは無視
					continue;
				}

				// セルのタイプが None（空）ならスキップ
				if (cell->type == BlockType::None ) {
					// 空のセルは描画しない
					continue;
				}

				if (cell->type == BlockType::PlayerStart && !isEditorMode_) {
					continue;
				}

				// ブロックがあるセルに対して、ブロックの種類に応じたモデルのオブジェクトを生成
				Vector3 position = {
					// ステージマップのセルの位置をワールド座標に変換してオブジェクトの位置とする
					static_cast<float>(x),
					static_cast<float>(y),
					static_cast<float>(z)
				};
				

				// ブロックの種類に応じて、対応するモデルを使ってオブジェクトを生成
				switch (cell->type) {
				// ブロックの種類が Ground（地面）の場合
				case BlockType::Ground:
				CreateStageObject(
					groundModel_.get(),
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Ground
				);
				break;

				// ブロックの種類が Wall（壁）の場合
				case BlockType::Wall:
				{
					Object3d* obj = CreateStageObject(
						wallModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::Wall
					);

					// ▼ 追加：Wallだけを透明化対象として保存
					if (obj) {
						wallObjects_.push_back(obj);
					}

					if (cell->variant >= 1 && cell->variant <= 5) {
						const auto* part = stageMap.GetCustomPart(cell->variant);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
						}
					} else if (cell->variant == 6) {
						// プレイヤー設置の Wall (可愛いライトレッド)
						obj->SetColor({ 1.0f, 0.4f, 0.4f, 1.0f });
					}
				}
				break;

				

				// ブロックの種類が Ladder（はしご）の場合
				case BlockType::Ladder:
				{
					Object3d* obj = CreateStageObject(
						ladderModel_.get(),
						position,
						blockScale_,
						{ 0.0f, cell->rotationY, 0.0f },
						BlockType::Ladder
					);
					// Ladderだけを透明化対象として保存
					if (cell->variant >= 1 && cell->variant <= 5) {
						const auto* part = stageMap.GetCustomPart(cell->variant);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
						}
					} else if (cell->variant == 7) {
						// プレイヤー設置の Ladder (可愛いライトグリーン)
						obj->SetColor({ 0.4f, 1.0f, 0.4f, 1.0f });
					}
				}
				break;

				// ブロックの種類が BubblePickup（泡の回収アイテム）の場合
				case BlockType::BubblePickup:
				{
					Object3d* obj = CreateStageObject(
						bubbleModel_.get(),
						position,
						{ blockScale_.x * 0.7f, blockScale_.y * 0.7f, blockScale_.z * 0.7f },
						{ 0.0f, 0.0f, 0.0f },
						BlockType::BubblePickup
					);
					int insideCustomId = UnpackBubbleCustomId(cell->variant);
					BlockType insideType = UnpackBubbleType(cell->variant);

					// カスタムパーツIDが1〜5の範囲内であれば、カスタムパーツの色を適用する
					if (insideCustomId >= 1 && insideCustomId <= 5) {
						const auto* part = stageMap.GetCustomPart(insideCustomId);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
						}
					} else {
						if (insideType == BlockType::Wall) {
							obj->SetColor({ 1.0f, 0.5f, 0.5f, 0.95f });
						} else if (insideType == BlockType::Ladder) {
							obj->SetColor({ 0.5f, 1.0f, 0.5f, 0.95f });
						} else {
							obj->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
						}
					}
				}
				break;

				// ブロックの種類が Goal（ゴール）の場合
				case BlockType::Goal:
				CreateStageObject(
					goalModel_.get(),
					position,
					{ blockScale_.x * 0.8f, blockScale_.y * 0.8f, blockScale_.z * 0.8f },
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Goal
				);
				break;

				// ブロックの種類が Star（仮のアイテム）の場合
				case BlockType::Star:
				CreateStageObject(
					wallModel_.get(),
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Star
				);
				break;
				// ブロックの種類が Door (ドア) の場合
				case BlockType::Door:
					CreateStageObject(
						doorModel_.get(),
						position,
						{ 0.6f, 0.6f, 0.6f },
						{ 0.0f, 0.0f, 0.0f },
						BlockType::Door
					);
					break;
					// ブロックの種類が PSwitch(Pスイッチ) の場合
				case BlockType::PSwitch:
				{
					Vector3 scale = { 0.6f, 0.6f, 0.6f };

					Object3d* obj = CreateStageObject(
						pSwichModel_.get(),
						position,
						scale,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::PSwitch
					);

					if (obj) {
						pSwitchObjects_.push_back({ obj, scale });
					}
				}
				break;

				// ブロックの種類が PBlock（Pブロック）の場合
				case BlockType::PBlock:

				// Pブロックは、押されて消えている状態と、実体化している状態の両方があるため、描画オブジェクトを生成する際に状態に応じて色を変える
				case BlockType::PBlockAppears:
				{
					Object3d* obj = CreateStageObject(
						pBlockOnModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						cell->type
					);

					if (obj) {
						if (!cell->isSolid) {
							// 押されて消えている状態（すり抜ける状態）は青色で半透明にする
							// ※マテリアルのアルファブレンドが有効になっている必要があります
							obj->SetColor({ 0.3f, 0.3f, 0.8f, 0.4f });
						}
						else {
							// 実体化している状態は元の色
							obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
						}

						// リストで管理している場合は追加
						if (cell->type == BlockType::PBlock) {
							pBlockObjects_.push_back({ obj, blockScale_ });
						}
					}
				}
				break;

				// ブロックの種類が TimedBlock（時間で消える足場）の場合
				case BlockType::TimedBlock:
				{
					Object3d* newObj = CreateStageObject(
						wallModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::TimedBlock
					);

					if (newObj) {
						TimedBlockInstance instance;
						instance.object = newObj;
						instance.cellIndex = { x, y, z };
						timedBlockInstances_.push_back(instance);
					}
				}
				break;

				// ブロックの種類が CrumblingFloor（崩れる足場）の場合
				case BlockType::CrumblingFloor:
				{
					Object3d* newObj = CreateStageObject(
						crumbleModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::CrumblingFloor
					);

					// 生成に成功したら専用の管理リストに記録する
					if (newObj) {
						CrumblingFloorInstance instance;
						instance.object = newObj;
						instance.cellIndex = { x, y, z };
						crumblingFloorInstances_.push_back(instance);
					}
				}
				break;
					// ブロックの種類が IceBlock（滑る足場）の場合
				case BlockType::IceBlock:
					CreateStageObject(
						iceBlockModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::IceBlock
					);
					break;
					// ブロックの種類が MovingFloor（動く足場）の場合
				case BlockType::MovingFloor:
				{
					// 1. 3Dオブジェクトを生成 (既存の他のブロックと同様の生成処理)
					Object3d* newObj = CreateStageObject(
						movingFloorModel_.get(),
						position,
						blockScale_,
						{ 0.0f, cell->rotationY, 0.0f },
						BlockType::MovingFloor
					);

					// 2. 生成に成功したら、更新用のリストに「オブジェクト」と「セルのインデックス」を記録
					if (newObj) {

						newObj->SetColor({ 1.0f,1.0f,1.0f,1.0f });

						MovingFloorInstance instance;
						instance.object = newObj;
						instance.cellIndex = { x, y, z }; // 現在ループで走査中の [x, y, z]
						movingFloorInstances_.push_back(instance);
					}
				}
					break;
					// 鍵の場合
				case BlockType::Key:
					CreateStageObject(
						keyModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f } // 必要に応じて回転
					);
					break;
					// ▼ 追加：鍵ブロックの場合
				case BlockType::KeyBlock:
					CreateStageObject(
						keyBlockModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f }
					);
					break;
				case BlockType::Checkpoint:
					CreateStageObject(
						checkpointModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f }
					);
					break;

				//トゲの場合
				case BlockType::Spike:
					{
						CreateStageObject(
							spikeModel_.get(),
							position,
							blockScale_,
							{ 0.0f, 0.0f, 0.0f },
							BlockType::Spike
						);
					}
					break;

				// ブロックの種類が EnemyWalker（歩く敵）の場合
				case BlockType::EnemyWalker:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(), // 球体モデルを流用
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyWalker
						);
						if (obj) {
							obj->SetColor({ 0.7f, 0.1f, 0.7f, 1.0f }); // 紫色の敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;

				// ブロックの種類が EnemyFlyer（飛行する敵）の場合
				case BlockType::EnemyFlyer:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(),
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyFlyer
						);
						if (obj) {
							obj->SetColor({ 0.8f, 0.8f, 0.1f, 1.0f }); // 黄色の敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;

				// ブロックの種類が EnemyChaser（追尾する敵）の場合
				case BlockType::EnemyChaser:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(),
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyChaser
						);
						if (obj) {
							obj->SetColor({ 0.1f, 0.8f, 0.8f, 1.0f }); // シアンの敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;

				// ブロックの種類が EnemyShooter（弾を撃つ敵）の場合
				case BlockType::OnOffSwitch: 
				{
					Object3d* obj = CreateStageObject(
						onOffSwichModel_.get(), // 🌟 専用モデルを使用
						position, blockScale_, { 0.0f, 0.0f, 0.0f }, BlockType::OnOffSwitch
					);
					if (obj) {
						if (stageMap.IsOnState()) {
							obj->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 最初はON（赤）
						}
						else {
							obj->SetColor({ 0.2f, 0.2f, 1.0f, 1.0f }); // OFF（青）
						}
					}
				} 
					break;

				// ブロックの種類が OnBlock（ONブロック）の場合
				case BlockType::OnBlock: 
				{
					Object3d* obj = CreateStageObject(
						onBlockModel_.get(), // 🌟 専用モデルを使用
						position, blockScale_, { 0.0f, 0.0f, 0.0f }, BlockType::OnBlock
					);
					if (obj) {
						if (stageMap.IsOnState()) {
							obj->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f }); // 最初はON（赤・不透明で出現）
						}
						else {
							obj->SetColor({ 1.0f, 0.2f, 0.2f, 0.3f }); // OFF（赤・透明で消滅）
						}
					}
				}
					break;

				// ブロックの種類が OffBlock（OFFブロック）の場合
				case BlockType::OffBlock: 
				{
					Object3d* obj = CreateStageObject(
						offBlockModel_.get(), // 🌟 専用モデルを使用
						position, blockScale_, { 0.0f, 0.0f, 0.0f }, BlockType::OffBlock
					);
					if (obj) {
						if (!stageMap.IsOnState()) {
							obj->SetColor({ 0.2f, 0.2f, 1.0f, 1.0f }); // 最初はOFF（青・不透明で出現）
						}
						else {
							obj->SetColor({ 0.2f, 0.2f, 1.0f, 0.3f }); // ON（青・透明で消滅）
						}
					}
				}
					break;

				// ブロックの種類が TransparentBlock（透明ブロック）の場合
				case BlockType::TransparentBlock:
				{
					Object3d* obj = CreateStageObject(
						wallModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::TransparentBlock
					);

					if (obj) {
						obj->SetColor({ 1.0f, 1.0f, 1.0f, 0.35f });
					}
				}
				break;

				// ブロックの種類が不明な場合は何もしない
				default:
				break;
				}
			}
		}
	}
	// リビルド後にインスタンス描画グループを再構築する
	RebuildTransparencyGroups();
}

// カメラ設定を全てのオブジェクトに伝える
void StageRenderer::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
	// 全てのオブジェクトに対して、カメラのビュー行列とプロジェクション行列を設定する
	for (const auto& obj : objects_) {
		obj->SetCamera(view, projection);
	}
	for (const auto& obj : previewObjects_) {
		obj->SetCamera(view, projection);
	}
	for (const auto& cloud : clouds_) {
		for (const auto& obj : cloud.objects) {
			obj->SetCamera(view, projection);
		}
	}
}

// 全てのオブジェクトの更新処理を呼び出す
void StageRenderer::Update(const StageMap& stageMap, const Matrix4x4& lightVP) {
	lastLightVP_ = lightVP;

	// -------------------------------------------------------------
	// ★追加：崩れる足場の毎フレーム演出更新（色・透明度・消去）
	// -------------------------------------------------------------
	for (auto& instance : crumblingFloorInstances_) {
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		if (cell && cell->type == BlockType::CrumblingFloor) {

			if (cell->isHidden) {
				// 【完全に消えている時】
				// アルファブレンド（半透明設定）が効かない環境でも、
				// スケールを 0 にすることで確実に画面から「消去」できます！
				instance.object->SetScale({ 0.0f, 0.0f, 0.0f });
			}
			else {
				// 【通常表示の時】元のサイズに戻し、タイマーに応じた色と透明度を設定
				instance.object->SetScale(blockScale_);
				instance.object->SetColor({
					1.0f,
					cell->colorG,
					cell->colorB,
					cell->opacity // ステージマップ側で計算した透明度
					});
			}

			// インスタンシング用の行列更新とDirtyフラグ立て
			instance.object->Update(lightVP);
			MarkDirty(instance.object);
		}
	}

	// ステージマップのセル情報を元に、動く床の位置を更新する
	for (auto& instance : movingFloorInstances_) {
		// ステージマップからセル情報を取得
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		// セルが存在し、かつ動く床のセルである場合のみ位置を更新
		if (cell && cell->type == BlockType::MovingFloor) {
			Vector3 basePosition = {
				static_cast<float>(instance.cellIndex.x) * blockScale_.x,
				static_cast<float>(instance.cellIndex.y) * blockScale_.y,
				static_cast<float>(instance.cellIndex.z) * blockScale_.z
			};

			Vector3 newPosition = {
				basePosition.x + (cell->currentOffsetX * blockScale_.x),
				basePosition.y + (cell->currentOffsetY * blockScale_.y),
				basePosition.z + (cell->currentOffsetZ * blockScale_.z)
			};

			instance.object->SetPosition(newPosition);
			instance.object->Update(lightVP); // 動く床のみ行列を更新
			// 更新した動く床のインスタンスデータをDirty化
			MarkDirty(instance.object);
		}
	}

	// ステージマップのセル情報を元に、敵の位置を更新する
	for (auto& instance : enemyInstances_) {
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		// セルが存在し、かつ敵のセルである場合のみ位置を更新
		if (cell && (cell->type == BlockType::EnemyWalker || cell->type == BlockType::EnemyFlyer || cell->type == BlockType::EnemyChaser)) {
			Vector3 basePosition = {
				static_cast<float>(instance.cellIndex.x) * blockScale_.x,
				static_cast<float>(instance.cellIndex.y) * blockScale_.y,
				static_cast<float>(instance.cellIndex.z) * blockScale_.z
			};

			Vector3 newPosition = {
				basePosition.x + (cell->currentOffsetX * blockScale_.x),
				basePosition.y + (cell->currentOffsetY * blockScale_.y),
				basePosition.z + (cell->currentOffsetZ * blockScale_.z)
			};

			instance.object->SetPosition(newPosition);
			instance.object->Update(lightVP);
			MarkDirty(instance.object);
		}
	}

	// ステージマップのセル情報を元に、時間で消える足場の表示状態を更新する
	for (auto& instance : timedBlockInstances_) {
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		if (cell && cell->type == BlockType::TimedBlock) {
			if (cell->isSolid) {
				// アクティブ状態：通常サイズで表示、鮮やかなオレンジ色
				instance.object->SetScale(blockScale_);
				instance.object->SetColor({ 1.0f, 0.6f, 0.1f, 1.0f });
			} else {
				// 非アクティブ状態：
				if (isEditorMode_) {
					// エディタモードなら半透明オレンジで表示
					instance.object->SetScale(blockScale_);
					instance.object->SetColor({ 1.0f, 0.6f, 0.1f, 0.3f });
				} else {
					// プレイモードなら非表示（スケール0）
					instance.object->SetScale({ 0.0f, 0.0f, 0.0f });
				}
			}
			instance.object->Update(lightVP);
			MarkDirty(instance.object);
		}
	}

	// 雲の更新
	float dt = 1.0f / 60.0f;
	int mapWidth = stageMap.GetWidth();
	float scaleX = blockScale_.x;
	float rightLimit = mapWidth * scaleX + 50.0f;
	float leftLimit = -50.0f;

	for (auto& cloud : clouds_) {
		// X方向への移動
		cloud.basePosition.x += cloud.speed.x * dt;
		if (cloud.basePosition.x > rightLimit) {
			cloud.basePosition.x = leftLimit;
		}

		// Y方向のフワフワ運動
		cloud.floatTimer += cloud.floatSpeed * dt;
		float offsetY = std::sin(cloud.floatTimer) * 0.4f;

		// 各パーツ（球体）の位置を更新
		for (size_t i = 0; i < cloud.objects.size(); ++i) {
			Vector3 partPos = {
				cloud.basePosition.x + cloud.localOffsets[i].x,
				cloud.basePosition.y + cloud.localOffsets[i].y + offsetY,
				cloud.basePosition.z + cloud.localOffsets[i].z
			};
			cloud.objects[i]->SetPosition(partPos);
			
			// 雲の色を完全に白に設定 (陰影のみライティングで反映される)
			cloud.objects[i]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			cloud.objects[i]->Update(lightVP);
		}
	}

	// ▼ この一行を追加して、ON / OFF状態を同期する
	ApplyOnOffVisualState(stageMap);
}

// 全てのオブジェクトの影描画処理を呼び出す
void StageRenderer::DrawShadow(const Matrix4x4& lightVP) {
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	if (!commandList) return;

	// インスタンシング用PSOの設定（影パス用）
	commandList->SetPipelineState(object3dCommon_->GetInstancedShadowPipelineState());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファの更新 (最初のグループからビュー、プロジェクション行列を取得)
	RenderGroup* firstGroup = nullptr;
	if (!renderGroups_.empty()) {
		firstGroup = &renderGroups_.front();
	} else if (!previewRenderGroups_.empty()) {
		firstGroup = &previewRenderGroups_.front();
	}

	// ビュー行列とプロジェクション行列を取得して定数バッファに設定
	if (firstGroup && !firstGroup->instances.empty() && viewProjectionData_) {
		Object3d* firstObj = firstGroup->instances.front().object;
		viewProjectionData_->viewProjection = Math::Multiply(firstObj->GetViewMatrix(), firstObj->GetProjectionMatrix());
		viewProjectionData_->lightViewProjection = lightVP;
	}

	// 1: ViewProjection
	commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());

	// 描画処理を実行するラムダ関数 (Dirtyフラグ制御によるメモリ転送の最小化)
	auto drawGroups = [commandList, this](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {
			UINT numInstances = static_cast<UINT>(group.instances.size());
			if (numInstances == 0) continue;

			// Dirtyならキャッシュ内容をGPUバッファに転送する
			if (group.isDirty) {
				InstanceData* dataBegin = nullptr;
				HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
				if (SUCCEEDED(hr)) {
					std::memcpy(dataBegin, group.instanceData.data(), sizeof(InstanceData) * numInstances);
					group.buffer->Unmap(0, nullptr);
				}
				group.isDirty = false; // 転送完了
			}

			// 5: InstanceBuffer (VS t2)
			commandList->SetGraphicsRootShaderResourceView(5, group.buffer->GetGPUVirtualAddress());

			// 頂点バッファをバインドして一括描画
			group.model->DrawInstanced(commandList, numInstances);
		}
	};

	// 影描画用のグループを描画
	drawGroups(renderGroups_);
	drawGroups(previewRenderGroups_);

	// 元の非インスタンシング影PSOに戻す
	commandList->SetPipelineState(object3dCommon_->GetShadowPipelineState());
}

// 全てのオブジェクトの透明描画処理を呼び出す
void StageRenderer::DrawTransparent()
{
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	if (!commandList) return;

	// インスタンシング用のテクスチャ記述子ヒープの設定
	if (object3dCommon_->GetTextureManager()) {
		ID3D12DescriptorHeap* heaps[] = { object3dCommon_->GetTextureManager()->GetSrvHeap() };
		commandList->SetDescriptorHeaps(1, heaps);
	}

	// インスタンシング用PSOの設定（透明描画用）
	commandList->SetPipelineState(object3dCommon_->GetInstancedAlphaPipelineState());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファの更新 (最初のグループからビュー、プロジェクション行列を取得)
	RenderGroup* firstGroup = nullptr;
	if (!renderGroups_.empty()) {
		firstGroup = &renderGroups_.front();
	} else if (!previewRenderGroups_.empty()) {
		firstGroup = &previewRenderGroups_.front();
	}

	// ビュー行列とプロジェクション行列を取得して定数バッファに設定
	if (firstGroup && !firstGroup->instances.empty() && viewProjectionData_) {
		Object3d* firstObj = firstGroup->instances.front().object;
		viewProjectionData_->viewProjection = Math::Multiply(firstObj->GetViewMatrix(), firstObj->GetProjectionMatrix());
		viewProjectionData_->lightViewProjection = lastLightVP_;
	}

	// 1: ViewProjection (VS b0 にバインド)
	commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
	// 2: Light (PS b1 にバインド)
	commandList->SetGraphicsRootConstantBufferView(2, object3dCommon_->GetLightGPUVirtualAddress());

	// 描画処理を実行するラムダ関数 (Dirtyフラグ制御によるメモリ転送の最小化)
	auto drawGroups = [commandList, this](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {

			// インスタンス数を取得
			UINT numInstances = static_cast<UINT>(group.instances.size());
			if (numInstances == 0) continue;

			// Dirtyならキャッシュ内容をGPUバッファに転送する
			if (group.isDirty) {
				InstanceData* dataBegin = nullptr;
				HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
				if (SUCCEEDED(hr)) {
					std::memcpy(dataBegin, group.instanceData.data(), sizeof(InstanceData) * numInstances);
					group.buffer->Unmap(0, nullptr);
				}
				group.isDirty = false; // 転送完了
			}

			// 3: Texture (PS t0)
			if (object3dCommon_->GetTextureManager()) {
				auto gpuHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(group.model->GetTextureHandle());
				commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
			}

			// 5: InstanceBuffer (VS t2)
			commandList->SetGraphicsRootShaderResourceView(5, group.buffer->GetGPUVirtualAddress());

			// 頂点バッファをバインドして一括描画
			group.model->DrawInstanced(commandList, numInstances);
		}
		};

	// 透明描画用のグループを描画
	drawGroups(transparentRenderGroups_);
	drawGroups(previewRenderGroups_);

	// 元の非インスタンシングPSOに戻す
	commandList->SetPipelineState(object3dCommon_->GetPipelineState());
}

// 全てのオブジェクトの描画処理を呼び出す
void StageRenderer::Draw() {
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	if (!commandList) return;

	// インスタンシング用のテクスチャ記述子ヒープの設定
	if (object3dCommon_->GetTextureManager()) {
		ID3D12DescriptorHeap* heaps[] = { object3dCommon_->GetTextureManager()->GetSrvHeap() };
		commandList->SetDescriptorHeaps(1, heaps);
	}

	// インスタンシング用PSOの設定
	commandList->SetPipelineState(object3dCommon_->GetInstancedPipelineState());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファの更新 (最初のグループからビュー、プロジェクション行列を取得)
	RenderGroup* firstGroup = nullptr;
	if (!renderGroups_.empty()) {
		firstGroup = &renderGroups_.front();
	} else if (!previewRenderGroups_.empty()) {
		firstGroup = &previewRenderGroups_.front();
	}

	if (firstGroup && !firstGroup->instances.empty() && viewProjectionData_) {
		Object3d* firstObj = firstGroup->instances.front().object;
		viewProjectionData_->viewProjection = Math::Multiply(firstObj->GetViewMatrix(), firstObj->GetProjectionMatrix());
		viewProjectionData_->lightViewProjection = lastLightVP_;
	}

	// 1: ViewProjection (VS b0 にバインド)
	commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
	// 2: Light (PS b1 にバインド)
	commandList->SetGraphicsRootConstantBufferView(2, object3dCommon_->GetLightGPUVirtualAddress());

	// 描画処理を実行するラムダ関数 (Dirtyフラグ制御によるメモリ転送の最小化)
	auto drawGroups = [commandList, this](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {

			
			UINT numInstances = static_cast<UINT>(group.instances.size());
			if (numInstances == 0) continue;

			// Dirtyならキャッシュ内容をGPUバッファに転送する
			if (group.isDirty) {
				InstanceData* dataBegin = nullptr;
				HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
				if (SUCCEEDED(hr)) {
					std::memcpy(dataBegin, group.instanceData.data(), sizeof(InstanceData) * numInstances);
					group.buffer->Unmap(0, nullptr);
				}
				group.isDirty = false; // 転送完了
			}

			// 3: Texture (PS t0)
			if (object3dCommon_->GetTextureManager()) {
				auto gpuHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(group.model->GetTextureHandle());
				commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
			}

			// 5: InstanceBuffer (VS t2)
			commandList->SetGraphicsRootShaderResourceView(5, group.buffer->GetGPUVirtualAddress());

			// 頂点バッファをバインドして一括描画
			group.model->DrawInstanced(commandList, numInstances);
		}
	};

	// 描画用のグループを描画
	drawGroups(renderGroups_);
	drawGroups(previewRenderGroups_);

	// 元の非インスタンシングPSOに戻す
	commandList->SetPipelineState(object3dCommon_->GetPipelineState());

	// 雲の描画
	for (const auto& cloud : clouds_) {
		for (const auto& obj : cloud.objects) {
			obj->Draw();
		}
	}
}

// 指定したモデルのインスタンス描画用バッファを取得、または必要に応じて作成する
ID3D12Resource* StageRenderer::GetOrCreateInstancedBuffer(Model* model, UINT numInstances) {
	auto& info = instancedBuffers_[model];
	// 既存のバッファがない、または必要なインスタンス数が現在の最大数を超える場合、新しいバッファを作成する
	if (!info.buffer || info.maxInstances < numInstances) {
		info.maxInstances = numInstances + 64;

		// D3D12_HEAP_PROPERTIES と D3D12_RESOURCE_DESC を使ってバッファを作成
		D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = sizeof(InstanceData) * info.maxInstances;
		resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

		// 既存のバッファがある場合は解放する
		HRESULT hr = object3dCommon_->GetDxCommon()->GetDevice()->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&info.buffer)
		);
		assert(SUCCEEDED(hr));
	}
	return info.buffer.Get();
}

// 既存のオブジェクトを全て削除してリストをクリアする
void StageRenderer::Clear() {
	activeObjectCount_ = 0; // objects_.clear(); avoided for pooling
	previewObjects_.clear(); // 🌟 プレビューも一緒にクリア
	crumblingFloorInstances_.clear();
	movingFloorInstances_.clear(); // ★追加：動く足場の管理リストもクリアしてダングリングポインタを防ぐ
	enemyInstances_.clear(); // ★追加：敵の管理リストもクリア
	clouds_.clear(); // ★追加：背景雲のリストもクリア
	timedBlockInstances_.clear(); // ★追加：時間差ブロックリストをクリア


	pSwitchObjects_.clear();
	pBlockObjects_.clear();

	wallObjects_.clear();

	// グループ管理データもクリア
	renderGroups_.clear();
	previewRenderGroups_.clear();
	objectToInstanceMap_.clear();

	instancedBuffers_.clear();

	activeObjectCount_ = 0;
}

void StageRenderer::ApplyOnOffVisualState(const StageMap& stageMap) {
	for (Object3d* obj : StageOnOffVisualController::Apply(stageMap, objects_)) {
		MarkDirty(obj);
	}
}

// 配置プレビュー表示機能の実装
void StageRenderer::SetPlacementPreview(
	const StageMap& stageMap,
	const Int3& cursorIndex,
	BlockType type,
	int customId,
	float rotationY
) {
	StagePlacementPreviewBuilder::Build(
		previewObjects_,
		object3dCommon_,
		{
			groundModel_.get(),
			wallModel_.get(),
			ladderModel_.get(),
			crumbleModel_.get(),
			iceBlockModel_.get(),
			movingFloorModel_.get()
		},
		blockScale_,
		stageMap,
		cursorIndex,
		type,
		customId,
		rotationY);
	BuildPreviewRenderGroups();
}

void StageRenderer::ClearPlacementPreview() {
	previewObjects_.clear();

	// プレビューグループもクリア
	previewRenderGroups_.clear();
}

// 指定したモデルと位置・スケール・回転を使ってオブジェクトを生成し、リストに追加して返す
Object3d* StageRenderer::CreateStageObject(
	Model* model,
	const Vector3& position,
	const Vector3& scale,
	const Vector3& rotation,
	BlockType type
) {
	Object3d* obj = nullptr;
	if (activeObjectCount_ >= objects_.size()) {
		auto newObj = std::make_unique<Object3d>();
		newObj->Initialize(object3dCommon_);
		obj = newObj.get();
		objects_.push_back(std::move(newObj));
	} else {
		obj = objects_[activeObjectCount_].get();
	}
	activeObjectCount_++;

	obj->SetModel(model);
	obj->SetPosition(position);
	obj->SetScale(scale);
	obj->SetRotation(rotation);
	obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 確実に色をリセット
	obj->SetShininess(0.3f);                   // パラメータを確実にリセット
	obj->SetMetallic(0.0f);                    // パラメータを確実にリセット
	obj->SetEmissive(0.0f);                    // 発光も確実にリセット
	
	// 高品質マイクロマテリアル設定の自動適用
	switch (type) {
	case BlockType::Ground:
		obj->SetShininess(0.3f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::Wall:
		obj->SetShininess(0.4f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		break;

	case BlockType::TransparentBlock:
		obj->SetShininess(0.4f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		obj->SetColor({ 1.0f, 1.0f, 1.0f, 0.35f });
		break;

	case BlockType::Ladder:
		obj->SetShininess(0.5f);
		obj->SetMetallic(0.2f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::IceBlock:
		obj->SetShininess(0.95f);
		obj->SetMetallic(0.7f);
		obj->SetEmissive(0.1f);
		break;
	case BlockType::Goal:
	case BlockType::Star:
		obj->SetShininess(0.8f);
		obj->SetMetallic(0.5f);
		obj->SetEmissive(0.7f);
		break;
	case BlockType::CrumblingFloor:
		obj->SetShininess(0.15f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::MovingFloor:
		obj->SetShininess(0.7f);
		obj->SetMetallic(0.4f);
		obj->SetEmissive(0.0f);
		obj->SetColor({ 1.0f,1.0f,1.0f,1.0f });
		break;
	case BlockType::PSwitch:
	case BlockType::PBlock:
		obj->SetShininess(0.8f);
		obj->SetMetallic(0.3f);
		obj->SetEmissive(0.4f);
		break;
	case BlockType::BubblePickup:
		obj->SetShininess(1.0f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(20.0f); // さらに強く発光させる（アルファ乗算負けしないように）
		break;
	default:
		break;
	}

	// 初期化後に行列を更新しておく
	obj->Update(Math::MakeIdentity4x4());
	return obj;
}

// PSwitchのON/OFF状態に応じて、PSwitchとPBlockの見た目を更新する
void StageRenderer::ApplyPSwitchVisualState(const StageMap& stageMap)
{
	for (Object3d* obj : StagePSwitchVisualController::Apply(
		stageMap.IsPSwitchActive(),
		pSwitchObjects_,
		pBlockObjects_)) {
		MarkDirty(obj);
	}
}

// --- 高速インスタンシング用：レンダーグループの構築 ---
void StageRenderer::BuildRenderGroups() {
	renderGroups_.clear();
	objectToInstanceMap_.clear();
	std::unordered_map<Model*, size_t> modelToGroupIndex;

	// 各オブジェクトをモデルごとにグループ化
	for (size_t i = 0; i < activeObjectCount_; ++i) {
		Object3d* obj = objects_[i].get();
		if (!obj || !obj->GetModel()) continue;

		// モデルごとにグループ化するためのマップを使用
		Model* model = obj->GetModel();
		auto it = modelToGroupIndex.find(model);
		size_t groupIndex = 0;

		// モデルがまだグループ化されていない場合、新しいグループを作成
		if (it == modelToGroupIndex.end()) {
			groupIndex = renderGroups_.size();
			modelToGroupIndex[model] = groupIndex;
			RenderGroup group;
			group.model = model;
			renderGroups_.push_back(std::move(group));
		} else {
			// 既存のグループがある場合、そのインデックスを取得
			groupIndex = it->second;
		}

		// インスタンス情報をグループに追加
		RenderInstance inst;
		inst.object = obj;
		inst.index = i;
		renderGroups_[groupIndex].instances.push_back(inst);
		
		// オブジェクトの生ポインタから逆引きマップへの登録
		objectToInstanceMap_[obj] = { groupIndex, renderGroups_[groupIndex].instances.size() - 1 };
	}

	// 各グループのデータをキャッシュに書き込み、初期バッファを作成
	for (auto& group : renderGroups_) {
		UINT numInstances = static_cast<UINT>(group.instances.size());
		group.instanceData.resize(numInstances);
		group.buffer = GetOrCreateInstancedBuffer(group.model, numInstances);
		group.maxInstances = numInstances;
		group.isDirty = true; // 初回は転送が必要

		// GPUバッファに初期データを転送
		InstanceData* dataBegin = nullptr;
		HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
		if (SUCCEEDED(hr)) {
			for (UINT i = 0; i < numInstances; ++i) {
				Object3d* obj = group.instances[i].object;
				const auto& tf = obj->GetTransform();
				group.instanceData[i].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

				const auto& mat = obj->GetMaterial();
				group.instanceData[i].color = mat.color;
				group.instanceData[i].shininess = mat.shininess;
				group.instanceData[i].metallic = mat.metallic;
				group.instanceData[i].emissive = mat.emissive;

				dataBegin[i] = group.instanceData[i];
			}
			group.buffer->Unmap(0, nullptr);
		}
		group.isDirty = false; // 初回データ転送完了
	}
}

// --- 高速インスタンシング用：プレビュー用レンダーグループの構築 ---
void StageRenderer::BuildPreviewRenderGroups() {
	previewRenderGroups_.clear();
	std::unordered_map<Model*, size_t> modelToGroupIndex;

	// 各プレビューオブジェクトをモデルごとにグループ化
	for (size_t i = 0; i < previewObjects_.size(); ++i) {
		Object3d* obj = previewObjects_[i].get();
		if (!obj || !obj->GetModel()) continue;

		// モデルごとにグループ化するためのマップを使用
		Model* model = obj->GetModel();
		auto it = modelToGroupIndex.find(model);
		size_t groupIndex = 0;
		// モデルがまだグループ化されていない場合、新しいグループを作成
		if (it == modelToGroupIndex.end()) {
			groupIndex = previewRenderGroups_.size();
			modelToGroupIndex[model] = groupIndex;
			RenderGroup group;
			group.model = model;
			previewRenderGroups_.push_back(std::move(group));
		} else {
			// 既存のグループがある場合、そのインデックスを取得
			groupIndex = it->second;
		}

		// インスタンス情報をグループに追加
		RenderInstance inst;
		inst.object = obj;
		inst.index = i;
		previewRenderGroups_[groupIndex].instances.push_back(inst);
	}

	// プレビューオブジェクト用のバッファ更新
	for (auto& group : previewRenderGroups_) {
		// インスタンス数を取得
		UINT numInstances = static_cast<UINT>(group.instances.size());
		group.instanceData.resize(numInstances);
		group.buffer = GetOrCreateInstancedBuffer(group.model, numInstances);
		group.maxInstances = numInstances;
		group.isDirty = true; // 初回は転送が必要

		// GPUバッファに初期データを転送
		InstanceData* dataBegin = nullptr;
		HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);

		// 転送が成功した場合、各インスタンスのデータをコピー
		if (SUCCEEDED(hr)) {
			for (UINT i = 0; i < numInstances; ++i) {
				Object3d* obj = group.instances[i].object;
				const auto& tf = obj->GetTransform();
				group.instanceData[i].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

				// マテリアル情報もコピー
				const auto& mat = obj->GetMaterial();
				group.instanceData[i].color = mat.color;
				group.instanceData[i].shininess = mat.shininess;
				group.instanceData[i].metallic = mat.metallic;
				group.instanceData[i].emissive = mat.emissive;

				// キャッシュデータをGPUバッファにコピー
				dataBegin[i] = group.instanceData[i];
			}
			group.buffer->Unmap(0, nullptr);
		}
		group.isDirty = false; // 転送完了
	}
}

// --- 高速インスタンシング用：変更されたオブジェクトのキャッシュ更新とDirty化 ---
void StageRenderer::MarkDirty(Object3d* obj) {
	auto it = objectToInstanceMap_.find(obj);
	if (it != objectToInstanceMap_.end()) {
		size_t groupIdx = it->second.first;
		size_t instIdx = it->second.second;
		auto& group = renderGroups_[groupIdx];

		// 範囲チェックをしてからキャッシュデータを更新
		if (instIdx < group.instanceData.size()) {
			const auto& tf = obj->GetTransform();
			group.instanceData[instIdx].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

			// マテリアル情報も更新
			const auto& mat = obj->GetMaterial();
			group.instanceData[instIdx].color = mat.color;
			group.instanceData[instIdx].shininess = mat.shininess;
			group.instanceData[instIdx].metallic = mat.metallic;
			group.instanceData[instIdx].emissive = mat.emissive;

			group.isDirty = true; // 次回の描画/影描画時にGPUへ再転送する
		}
	}
}

// --- 高速インスタンシング用：透明描画グループの再構築 ---
void StageRenderer::RebuildTransparencyGroups()
{
	renderGroups_.clear();
	transparentRenderGroups_.clear();
	objectToInstanceMap_.clear();

	// オブジェクトをモデルごとにグループ化するラムダ関数
	auto AddToGroups = [&](std::vector<RenderGroup>& groups, Object3d* obj, size_t index) {
		Model* model = obj->GetModel();

		// 既存のグループを検索
		RenderGroup* targetGroup = nullptr;
		// 既存のグループが見つからなければ新しいグループを作成
		for (auto& group : groups) {
			if (group.model == model) {
				targetGroup = &group;
				break;
			}
		}

		// 既存のグループが見つからなければ新しいグループを作成
		if (!targetGroup) {
			RenderGroup group;
			group.model = model;
			groups.push_back(std::move(group));
			targetGroup = &groups.back();
		}

		// インスタンス情報をグループに追加
		RenderInstance inst;
		inst.object = obj;
		inst.index = index;

		// インスタンスのインデックスを取得してから追加
		size_t instanceIndex = targetGroup->instances.size();
		targetGroup->instances.push_back(inst);

		// ★通常描画グループだけ MarkDirty 用マップに登録
		if (&groups == &renderGroups_) {
			size_t groupIndex = 0;
			for (size_t i = 0; i < renderGroups_.size(); ++i) {
				if (&renderGroups_[i] == targetGroup) {
					groupIndex = i;
					break;
				}
			}
			objectToInstanceMap_[obj] = { groupIndex, instanceIndex };
		}
	};

	// MovingFloorオブジェクトかどうかを判定するラムダ関数
	auto IsMovingFloorObject = [&](Object3d* obj) {
		for (const auto& instance : movingFloorInstances_) {
			if (instance.object == obj) {
				return true;
			}
		}
		return false;
	};

	// 全てのオブジェクトを走査して、透明か不透明かでグループ分けする
	for (size_t i = 0; i < activeObjectCount_; ++i) {
		Object3d* obj = objects_[i].get();
		if (!obj || !obj->GetModel()) continue;

		// ★最重要：MovingFloorは絶対に通常描画
		if (IsMovingFloorObject(obj)) {
			obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			AddToGroups(renderGroups_, obj, i);
			continue;
		}

		// マテリアルのアルファ値を取得して透明か不透明かを判定
		const auto& mat = obj->GetMaterial();

		// 透明度が0.99未満なら透明描画グループに追加、そうでなければ通常描画グループに追加
		if (mat.color.w < 0.99f) {
			AddToGroups(transparentRenderGroups_, obj, i);
		} else {
			AddToGroups(renderGroups_, obj, i);
		}
	}

	// 各グループのインスタンスデータを構築するラムダ関数
	auto BuildGroupData = [&](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {
			UINT numInstances = static_cast<UINT>(group.instances.size());
			group.instanceData.resize(numInstances);
			
			// 既存のバッファがない、または必要なインスタンス数が現在の最大数を超える場合、新しいバッファを作成
			if (!group.buffer || group.maxInstances < numInstances) {
				group.maxInstances = numInstances + 64;

				// D3D12_HEAP_PROPERTIES と D3D12_RESOURCE_DESC を使ってバッファを作成
				D3D12_HEAP_PROPERTIES heapProps = {
					D3D12_HEAP_TYPE_UPLOAD,
					D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
					D3D12_MEMORY_POOL_UNKNOWN,
					1,
					1
				};

				// バッファのリソース記述子を設定
				D3D12_RESOURCE_DESC resDesc = {};
				resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				resDesc.Width = sizeof(InstanceData) * group.maxInstances;
				resDesc.Height = 1;
				resDesc.DepthOrArraySize = 1;
				resDesc.MipLevels = 1;
				resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resDesc.SampleDesc.Count = 1;

				// 既存のバッファがある場合は解放する
				HRESULT hr = object3dCommon_->GetDxCommon()->GetDevice()->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&resDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&group.buffer)
				);

				assert(SUCCEEDED(hr));
			}

			// Dirtyフラグを立てて、次回の描画時にGPUバッファに転送する
			group.isDirty = true;

			// GPUバッファに初期データを転送
			InstanceData* dataBegin = nullptr;
			HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);

			// 転送が成功した場合、各インスタンスのデータをコピー
			if (SUCCEEDED(hr)) {
				for (UINT i = 0; i < numInstances; ++i) {
					Object3d* obj = group.instances[i].object;
					const auto& tf = obj->GetTransform();
					const auto& mat = obj->GetMaterial();

					group.instanceData[i].world =
						Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);
					group.instanceData[i].color = mat.color;
					group.instanceData[i].shininess = mat.shininess;
					group.instanceData[i].metallic = mat.metallic;
					group.instanceData[i].emissive = mat.emissive;

					dataBegin[i] = group.instanceData[i];
				}
				group.buffer->Unmap(0, nullptr);
			}

			group.isDirty = false;
		}
	};

	BuildGroupData(renderGroups_);
	BuildGroupData(transparentRenderGroups_);
}

// 壁の透明化処理を更新する
void StageRenderer::UpdateWallTransparency(
	const Vector3& cameraPos,
	const Vector3& playerPos,
	bool enableTransparency,
	float transparencyAlpha,
	int currentStageIndex
)
{
	// 壁の透明化処理を適用する
	StageWallTransparencyController::Apply(
		wallObjects_,
		cameraPos,
		playerPos,
		enableTransparency,
		transparencyAlpha,
		currentStageIndex);
	RebuildTransparencyGroups();
}

// 雲の透明化処理を更新する
void StageRenderer::UpdateCloudTransparency(
	const Vector3& cameraPos,
	const Vector3& playerPos
)
{
	// カメラから自機への視線ベクトルを計算
	Vector3 viewLine = {
		playerPos.x - cameraPos.x,
		playerPos.y - cameraPos.y,
		playerPos.z - cameraPos.z
	};

	// 視線ベクトルの長さの二乗を計算
	float lineLenSq =
		viewLine.x * viewLine.x +
		viewLine.y * viewLine.y +
		viewLine.z * viewLine.z;

	// 視線ベクトルがほぼゼロの場合は処理をスキップ
	if (lineLenSq <= 0.0001f) {
		return;
	}

	// 雲のリストを走査して、視線に近い雲を透明化する
	for (auto& cloud : clouds_) {
		for (size_t i = 0; i < cloud.objects.size(); ++i) {
			Object3d* obj = cloud.objects[i].get();

			// 雲オブジェクトが存在しない場合はスキップ
			if (!obj) {
				continue;
			}

			// 雲の位置を取得
			Vector3 cloudPos = obj->GetPosition();

			Vector3 cameraToCloud = {
				cloudPos.x - cameraPos.x,
				cloudPos.y - cameraPos.y,
				cloudPos.z - cameraPos.z
			};

			// カメラ→自機の線分上のどの位置に雲が近いか
			float t = (cameraToCloud.x * viewLine.x +
					cameraToCloud.y * viewLine.y +
					cameraToCloud.z * viewLine.z) / lineLenSq;

			// カメラより手前、または自機より奥は対象外
			if (t < 0.0f || t > 1.0f) {
				obj->SetScale(cloud.localScales[i]);
				continue;
			}

			// 視線上の最近接点を計算
			Vector3 closestPoint = {
				cameraPos.x + viewLine.x * t,
				cameraPos.y + viewLine.y * t,
				cameraPos.z + viewLine.z * t
			};

			float dx = cloudPos.x - closestPoint.x;
			float dy = cloudPos.y - closestPoint.y;
			float dz = cloudPos.z - closestPoint.z;

			float distSqToViewLine = dx * dx + dy * dy + dz * dz;

			// 視線に近い雲だけ消す
			if (distSqToViewLine < 16.0f) {
				obj->SetScale({ 0.0f, 0.0f, 0.0f });
			} else {
				obj->SetScale(cloud.localScales[i]);
			}
		}
	}
}

