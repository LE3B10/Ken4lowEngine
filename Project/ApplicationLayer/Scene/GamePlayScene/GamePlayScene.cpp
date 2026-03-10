#define NOMINMAX
#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include "SkyBoxManager.h"
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include "LevelLoader.h"

#include "GpuParticleManager.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleType.h"
#include "BillboardMode.h"

#include "StageRepository.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <cmath>

#include "Player.h"

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <filesystem>

#include <algorithm>

//#include "LinearInterpolation.h"

using namespace Ken4lowEngine;

namespace
{
	constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

	std::vector<StageInfo> BuildDefaultStages()
	{
		std::vector<StageInfo> stages;
		stages.push_back({ 0u, "始まりの森",      "white.png", false, 0u, { 0.18f, 0.49f, 0.20f, 1.0f } });
		stages.push_back({ 1u, "廃鉱山",          "white.png", true,  0u, { 0.43f, 0.30f, 0.25f, 1.0f } });
		stages.push_back({ 2u, "工業地帯",        "white.png", true,  0u, { 0.96f, 0.49f, 0.00f, 1.0f } });
		stages.push_back({ 3u, "朽ちた果てた街",  "white.png", true,  0u, { 0.37f, 0.35f, 0.49f, 1.0f } });
		stages.push_back({ 4u, "港湾ターミナル",  "white.png", true,  0u, { 0.08f, 0.40f, 0.75f, 1.0f } });
		return stages;
	}

	bool IsPlayerSpawnType(const std::string& type)
	{
		return type == "PlayerSpawnPoint";
	}

	bool IsEnemySpawnType(const std::string& type)
	{
		return type == "EnemySpawnPoint";
	}

	bool IsBossSpawnType(const std::string& type)
	{
		return type == "BossSpawnPoint";
	}

	K4E::Vector3 LerpVec3(const K4E::Vector3& a, const K4E::Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	float LerpFloat(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	float CatmullRomFloat(float p0, float p1, float p2, float p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
			);
	}

	K4E::Vector3 CatmullRomVec3(
		const K4E::Vector3& p0,
		const K4E::Vector3& p1,
		const K4E::Vector3& p2,
		const K4E::Vector3& p3,
		float t)
	{
		return {
			CatmullRomFloat(p0.x, p1.x, p2.x, p3.x, t),
			CatmullRomFloat(p0.y, p1.y, p2.y, p3.y, t),
			CatmullRomFloat(p0.z, p1.z, p2.z, p3.z, t),
		};
	}

	const GamePlayScene::IntroCameraPointInfo& GetIntroPointClamped(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		int index)
	{
		const int maxIndex = static_cast<int>(points.size()) - 1;
		index = std::clamp(index, 0, maxIndex);
		return points[static_cast<size_t>(index)];
	}

	K4E::Vector3 FindLookAtPosition(
		const std::vector<GamePlayScene::IntroLookAtPointInfo>& lookPoints,
		const std::string& name,
		const K4E::Vector3& fallback)
	{
		for (const auto& p : lookPoints)
		{
			if (p.name == name)
			{
				return p.position;
			}
		}
		return fallback;
	}

	K4E::Vector3 EulerDegToForward(const K4E::Vector3& rotDeg)
	{
		const float pitch = rotDeg.x * kDegToRad;
		const float yaw = rotDeg.y * kDegToRad;

		const float cp = std::cos(pitch);
		const float sp = std::sin(pitch);
		const float cy = std::cos(yaw);
		const float sy = std::sin(yaw);

		K4E::Vector3 forward{
			sy * cp,
			-sp,
			cy * cp
		};

		return K4E::Vector3::Normalize(forward);
	}

	K4E::Vector3 BuildAimTargetFromPoint(
		const GamePlayScene::IntroCameraPointInfo& point,
		const std::vector<GamePlayScene::IntroLookAtPointInfo>& lookPoints)
	{
		if (point.aimMode == "Euler")
		{
			const K4E::Vector3 forward = EulerDegToForward(point.rotation);
			return point.position + forward * 10.0f;
		}

		return FindLookAtPosition(
			lookPoints,
			point.targetName,
			point.position + K4E::Vector3{ 0.0f, 0.0f, 1.0f });
	}

	K4E::Vector3 EvaluateIntroPosition(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomVec3(p0.position, p1.position, p2.position, p3.position, t);
		}

		return LerpVec3(p1.position, p2.position, t);
	}

	K4E::Vector3 EvaluateIntroTarget(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		const std::vector<GamePlayScene::IntroLookAtPointInfo>& lookPoints,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		const K4E::Vector3 a0 = BuildAimTargetFromPoint(p0, lookPoints);
		const K4E::Vector3 a1 = BuildAimTargetFromPoint(p1, lookPoints);
		const K4E::Vector3 a2 = BuildAimTargetFromPoint(p2, lookPoints);
		const K4E::Vector3 a3 = BuildAimTargetFromPoint(p3, lookPoints);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomVec3(a0, a1, a2, a3, t);
		}

		return LerpVec3(a1, a2, t);
	}

	float EvaluateIntroFov(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomFloat(p0.fov, p1.fov, p2.fov, p3.fov, t);
		}

		return LerpFloat(p1.fov, p2.fov, t);
	}

	constexpr float kPi = std::numbers::pi_v<float>;

	float WrapAngleRad(float angle)
	{
		while (angle > kPi) { angle -= 2.0f * kPi; }
		while (angle < -kPi) { angle += 2.0f * kPi; }
		return angle;
	}

	float LerpAngleRad(float a, float b, float t)
	{
		const float delta = WrapAngleRad(b - a);
		return WrapAngleRad(a + delta * t);
	}

	K4E::Vector3 LerpEulerRad(const K4E::Vector3& a, const K4E::Vector3& b, float t)
	{
		return {
			LerpAngleRad(a.x, b.x, t),
			LerpAngleRad(a.y, b.y, t),
			LerpAngleRad(a.z, b.z, t)
		};
	}

	K4E::Vector3 EulerRadToForward(const K4E::Vector3& rotRad)
	{
		const float pitch = rotRad.x;
		const float yaw = rotRad.y;

		const float cp = std::cos(pitch);
		const float sp = std::sin(pitch);
		const float cy = std::cos(yaw);
		const float sy = std::sin(yaw);

		// Yaw の向きを反転
		K4E::Vector3 forward{
			sy * cp,
			-sp,
			cy * cp
		};

		return K4E::Vector3::Normalize(forward);
	}

	K4E::Vector3 DirectionToEulerRad(const K4E::Vector3& dir)
	{
		K4E::Vector3 n = K4E::Vector3::Normalize(dir);

		// Yaw の向きを反転
		const float yaw = std::atan2(-n.x, n.z);
		const float horizontalLen = std::sqrt(n.x * n.x + n.z * n.z);
		const float pitch = std::atan2(-n.y, horizontalLen);

		return { pitch, yaw, 0.0f };
	}

	K4E::Vector3 LookAtToEulerRad(const K4E::Vector3& from, const K4E::Vector3& to)
	{
		return DirectionToEulerRad(to - from);
	}

	K4E::Vector3 EvaluateIntroEulerRotation(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			K4E::Vector3 rot = CatmullRomVec3(p0.rotation, p1.rotation, p2.rotation, p3.rotation, t);
			rot.x = WrapAngleRad(rot.x);
			rot.y = WrapAngleRad(rot.y);
			rot.z = WrapAngleRad(rot.z);
			return rot;
		}

		return LerpEulerRad(p1.rotation, p2.rotation, t);
	}

	K4E::Vector3 EvaluateIntroCameraRotation(
		const std::vector<GamePlayScene::IntroCameraPointInfo>& points,
		const std::vector<GamePlayScene::IntroLookAtPointInfo>& lookPoints,
		int segmentIndex,
		float t,
		const K4E::Vector3& camPos)
	{
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);

		// Euler 指定なら Blender 側の回転をそのまま補間して使う
		if (p1.aimMode == "Euler" || p2.aimMode == "Euler")
		{
			return EvaluateIntroEulerRotation(points, segmentIndex, t);
		}

		// Target 指定なら注視点から回転を逆算する
		const K4E::Vector3 lookPos = EvaluateIntroTarget(points, lookPoints, segmentIndex, t);
		return LookAtToEulerRad(camPos, lookPos);
	}
}

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	K4E::DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	LightManager::GetInstance()->AddDefaultDirectionalLight();

	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	input_->SetLockCursor(true);
	input_->SetCursorVisible(false);

	skyBox_ = std::make_unique<K4E::SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	// 衝突マネージャーの初期化
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// 弾丸マネージャーの初期化
	bulletManager_ = std::make_unique<BulletManager>();
	bulletManager_->Initialize(collisionManager_.get());

	// キャラクター関連の初期化
	GameContext ctx{};
	ctx.collisionManager_ = collisionManager_.get();
	ctx.bulletManager_ = bulletManager_.get();
	characters_.Initialize(ctx);

	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->SetPlayer(characters_.GetPlayer());
	hudManager_->Initialize();
	characters_.GetPlayer()->SetHUDManager(hudManager_.get());

	// ポーズメニューの初期化
	pauseMenu_ = std::make_unique<PauseMenu>();
	pauseMenu_->Initialize();

	// 結果メニューの初期化
	resultMenu_ = std::make_unique<ResultMenu>();
	resultMenu_->Initialize();

	// ステージ選択で保存された開始インデックスを取得
	auto& repo = StageRepository::GetInstance();

	// Repository にステージ一覧が無ければデフォルトを作る
	auto stages = repo.GetStages();
	if (stages.empty())
	{
		stages = BuildDefaultStages();
		repo.SetStages(stages);
	}

	// 開始ステージ取得
	int stageIndex = repo.GetStartIndex().value_or(0);

	// 保存済みステージ数に合わせてクランプ
	if (stageIndex < 0)
	{
		stageIndex = 0;
	}
	if (!stages.empty() && stageIndex >= static_cast<int>(stages.size()))
	{
		stageIndex = static_cast<int>(stages.size()) - 1;
	}

	// 現在ステージとして保持
	currentStageIndex_ = stageIndex;

	// 念のため Repository にも反映
	repo.SetStartIndex(currentStageIndex_);

	const StageAssetPaths stageAssets = GetStageAssetPaths(currentStageIndex_);

	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize(stageAssets.jsonPath, stageAssets.modelPath);
	stage_->RegisterColliders(collisionManager_.get());

	// ------------------------------------------------------------
	// 先にステージ側を1回更新して、ワールド衝突情報を作る
	// ------------------------------------------------------------
	stage_->Update();

	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());

	Player* player = characters_.GetPlayer();
	if (player)
	{
		player->SetStageWorldAABBs(&stage_->GetWorldAABBs());

		// Playerの衝突サイズを設定
		WorldCollisionSettings playerCollisionSettings{};
		playerCollisionSettings.half = { 0.5f, 1.0f, 0.5f };
		playerCollisionSettings.centerOffset = { 0.0f, 1.0f, 0.0f };
		player->SetWorldCollisionSettings(playerCollisionSettings);
	}

	// ------------------------------------------------------------
	// レベルJSONからスポーンポイントを読み込む
	// ------------------------------------------------------------
	LoadSpawnPointsFromLevel(stageAssets.jsonPath);

	// ------------------------------------------------------------
	// プレイヤー開始位置を反映
	// ★ wt->translate_ の直書きはやめて、
	//   SetSpawnPosition / SetSpawnOffset で内部中心座標まで同期する
	// ------------------------------------------------------------
	if (player)
	{
		player->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

		if (hasPlayerSpawnPoint_)
		{
			constexpr float kPlayerSpawnLift = 1.0f;

			K4E::Vector3 spawn = playerSpawnPoint_;
			spawn.y += kPlayerSpawnLift;
			player->SetSpawnPosition(spawn);
		}

		// スポーン直後に1回だけ同期更新
		characters_.Update(0.0f);
	}

	// ------------------------------------------------------------
	// スポーン後にもう一度ステージ / 衝突を更新
	// ------------------------------------------------------------
	if (stage_)
	{
		stage_->Update();
	}

	CollisionUpdate();

	isPaused_ = false;
	resultInputCooldown_ = 0.0f;

	waveManager_ = std::make_unique<WaveManager>();
	SetupWaves();

	prevWaveNumber_ = 0;
	prevWaveInProgress_ = false;
	prevAllWavesCleared_ = false;

	// ------------------------------------------------------------
	// Intro がある時だけ Intro 開始
	// 無い時は即 Playing にして Wave 開始
	// ------------------------------------------------------------
	isIntroStarted_ = false;
	introTimer_ = 0.0f;
	introCurrentSegment_ = 0;
	introSegmentTimer_ = 0.0f;

	const bool hasIntroPoints = !introCameraPoints_.empty();

	if (hasIntroPoints)
	{
		gameFlowState_ = GameFlowState::Intro;
		isIntroStarted_ = true;
		introTimer_ = introDuration_;
	}
	else
	{
		gameFlowState_ = GameFlowState::Playing;

		if (waveManager_)
		{
			waveManager_->Start();
		}
	}
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// デルタタイムの取得
	const float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

#ifdef _DEBUG // デバッグビルドのみ

	// ------------------------------------------------------------
	// ImGui操作用 完全停止トグル (F1)
	// ------------------------------------------------------------
	if (input_ && input_->TriggerKey(DIK_F1))
	{
		if (isImGuiFreeze_)
		{
			ExitImGuiFreeze();
		}
		else
		{
			EnterImGuiFreeze();
		}
		return; // 切替フレームはここで終了
	}

	// 完全停止中は ImGui 以外のゲーム更新を全部止める
	if (isImGuiFreeze_)
	{
		UpdateImGuiFreeze();
		return;
	}
#endif // _DEBUG

	// ------------------------------------------------------------
	// 開始演出中
	// ------------------------------------------------------------
	if (gameFlowState_ == GameFlowState::Intro)
	{
		UpdateIntro(deltaTime);
		return;
	}

	// ------------------------------------------------------------
	// ゲームクリア / ゲームオーバー中
	// ------------------------------------------------------------
	if (gameFlowState_ == GameFlowState::GameClear ||
		gameFlowState_ == GameFlowState::GameOver)
	{
		UpdateResult(deltaTime);
		return;
	}

	// ------------------------------------------------------------
	// Pause toggle (ESC)
	// ------------------------------------------------------------
	if (input_ && input_->TriggerKey(DIK_ESCAPE))
	{
		if (isPaused_)
		{
			ExitPause();
		}
		else
		{
			EnterPause();
		}
		return;
	}

	if (isPaused_)
	{
		UpdatePaused(deltaTime);
		return;
	}

	UpdateDebug();

	stage_->Update();

	characters_.Update(deltaTime);

	UpdateShadowLightViewProjection();
	stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	if (bulletManager_)
	{
		bulletManager_->Update(deltaTime);
	}

	CollisionUpdate();

	if (skyBox_) skyBox_->Update();

	if (hudManager_ && characters_.GetPlayer())
	{
		hudManager_->SetHP(characters_.GetPlayer()->GetHP(), characters_.GetPlayer()->GetMaxHP());
		hudManager_->Update(deltaTime);
	}

	if (characters_.GetPlayer() && characters_.GetPlayer()->GetHP() <= 0)
	{
		EnterGameOver();
		return;
	}

	if (waveManager_)
	{
		waveManager_->Update(characters_, deltaTime);

		const int currentWave = waveManager_->GetCurrentWaveNumber();
		const int totalWaves = waveManager_->GetTotalWaveCount();
		const bool isWaveInProgress = waveManager_->IsWaveInProgress();
		const bool isWaitingNextWave = waveManager_->IsWaitingNextWave();
		const bool isAllWavesCleared = waveManager_->IsAllWavesCleared();
		const bool isFinalWave = (currentWave >= totalWaves);

		if (hudManager_)
		{
			WaveUI::DisplayState state{};
			state.currentWave = currentWave;
			state.totalWaves = totalWaves;
			state.isWaveInProgress = isWaveInProgress;
			state.isWaitingNextWave = isWaitingNextWave;
			state.isAllWavesCleared = isAllWavesCleared;

			hudManager_->SetWaveDisplayState(state);

			if (isWaveInProgress && (!prevWaveInProgress_ || currentWave != prevWaveNumber_))
			{
				hudManager_->NotifyWaveStarted(currentWave, isFinalWave);
			}

			if (isAllWavesCleared && !prevAllWavesCleared_)
			{
				hudManager_->NotifyAllWavesCleared();
			}
		}

		prevWaveNumber_ = currentWave;
		prevWaveInProgress_ = isWaveInProgress;
		prevAllWavesCleared_ = isAllWavesCleared;

		if (isAllWavesCleared)
		{
			EnterGameClear();
			return;
		}
	}
}

/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	K4E::SkyBoxManager::GetInstance()->SetRenderSetting();

	skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	const bool hideCharactersDuringIntro = (gameFlowState_ == GameFlowState::Intro);

	// キャラクターの描画
	// カメラ演出中はキャラクターを描画しない
	if (!hideCharactersDuringIntro)
	{
		characters_.Draw();
	}

	if (stage_) { stage_->Draw(); }

	// 弾丸の描画
	if (bulletManager_) { bulletManager_->Draw(); }

#pragma endregion


#pragma region アニメーションモデルの描画

#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	if (collisionManager_) { collisionManager_->Draw(); }

	// FPSカメラの描画
	//fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	K4E::Wireframe::GetInstance()->DrawGrid(200.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });
#endif // _DEBUG
}

void GamePlayScene::DrawShadowObjects()
{
	if (stage_) { stage_->DrawShadow(); } // もし Stage 側にあるなら
	const bool hideCharactersDuringIntro = (gameFlowState_ == GameFlowState::Intro);

	// カメラ演出中はキャラクターの影も描画しない
	if (!hideCharactersDuringIntro)
	{
		characters_.DrawShadow();
	}
}

/// -------------------------------------------------------------
///				　		2Dスプライトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();

	const bool hideCharactersDuringIntro = (gameFlowState_ == GameFlowState::Intro);


	if (!hideCharactersDuringIntro) { hudManager_->Draw(); }


	if (isPaused_ && pauseMenu_) { pauseMenu_->Draw(); }

	if ((gameFlowState_ == GameFlowState::GameClear ||
		gameFlowState_ == GameFlowState::GameOver) &&
		resultMenu_)
	{
		resultMenu_->Update();
		resultMenu_->Draw();
	}

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}

	waveManager_.reset();

	stage_.reset();

	hudManager_.reset();
	pauseMenu_.reset();
	resultMenu_.reset();

	// ★重要：CharacterWorld は CollisionManager を使って RemoveCollider する
	//         ので、先に characters_ を Finalize してから manager 類を破棄する
	characters_.Finalize();

	// 弾丸マネージャーの終了処理（Collision を参照している可能性があるため先）
	bulletManager_.reset();

	// 衝突マネージャーの終了処理
	if (collisionManager_) {
		collisionManager_->Reset();
	}
	collisionManager_.reset();

	// 3D背景など
	skyBox_.reset();

	// 生ポインタ参照は最後に切る
	input_ = nullptr;
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
#ifdef USE_IMGUI

	// ライト
	K4E::LightManager::GetInstance()->DrawImGui();


	characters_.DrawImGui();

	/// ---------- 武器マスターデータエディタ ---------- ///
	static WeaponMasterDataDatabase weaponDB;
	static WeaponMasterDataEditor weaponEditor;
	static WeaponEditorHooks hooks;
	static bool initialized = false;
	static int32_t lastAppliedID = 0;

	static const std::filesystem::path kRoot = "Resources/JSON/weapons";

	if (!initialized)
	{
		initialized = true;

		// ★ ここでロードする（既存jsonを表示したいなら必須）
		// 空から始めたいなら LoadFromDirectory をコメントアウトしてOK
		{
			std::string err;
			weaponDB.LoadFromDirectory(kRoot, &err);
			// errをImGuiに出したいなら保持して表示
		}

		hooks.SaveAll = [&]()
			{
				std::string err;
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, kRoot, &err);

				// ★ 保存後、実際のプレイヤー武器を再読込して反映
				if (auto* player = characters_.GetPlayer())
				{
					// ↓ アクセサ名は実装に合わせて変更
					player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
				}
			};

		hooks.RequestReloadFocus = [](int32_t) {};

		hooks.RebuildLoadout = [&]()
			{
				if (auto* player = characters_.GetPlayer())
				{
					// ↓ アクセサ名は実装に合わせて変更
					player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
				}
			};

		hooks.ApplyToRuntimeIfCurrent =
			[&](int32_t weaponID, const FWeaponMasterData&)
			{
				lastAppliedID = weaponID;

				// Editor DB と runtime DB は別なので、Applyでも一旦保存してから再読込する
				std::string err;
				WeaponMasterDataWriter::SaveAllByCategory(weaponDB, kRoot, &err);

				if (auto* player = characters_.GetPlayer())
				{
					// ↓ アクセサ名は実装に合わせて変更
					auto& wc = player->GetWeaponComponent();

					// 現在装備中IDだけ再反映したいならこれ
					if (wc.GetCurrentWeaponId() == weaponID)
					{
						wc.ReloadWeaponMasterDataAndReequip();
					}

					if (player)
					{
						player->GetWeaponComponent().ReloadWeaponMasterDataAndReequip();
						player->ForceRefreshWeaponVisual();
					}
				}
			};

		hooks.RequestDelete =
			[&](int32_t weaponID)
			{
				std::string err;

				// ディスク上のjson削除
				WeaponMasterDataWriter::DeleteFilesByWeaponID(kRoot, weaponID, &err);

				// DBから削除
				weaponDB.RemoveByID(weaponID);
			};

		hooks.RequestAdd = [](const std::string&, int32_t) {}; // Editor側はDB直操作なので空でOK

		// ★ これがあると毎回空になる。1枚目が「常に0」なのはこれが原因。
		// weaponDB.Clear();

		hooks.PlaySoundPreviewSE = [](const std::string& path)
			{
				if (path.empty()) return;

				// まずはSEとしてワンショット再生
				K4E::AudioManager::GetInstance()->PlayBGM(path, 1.0f, 1.0f, false);
			};

		hooks.GetImagePreview = [](const std::string& path)
			{
				WeaponEditorImagePreview out{};
				if (path.empty()) return out;

				// パスの区切りを統一
				std::string normalized = path;
				for (char& c : normalized) if (c == '\\') c = '/';

				std::error_code ec;
				if (!std::filesystem::exists(normalized, ec))
				{
					OutputDebugStringA(("[GetImagePreview] file not found: " + normalized + "\n").c_str());
					return out;
				}

				auto* texMgr = K4E::TextureManager::GetInstance();
				if (!texMgr)
				{
					OutputDebugStringA("[GetImagePreview] TextureManager is null\n");
					return out;
				}

				// ここで未ロードなら自動ロードされる
				auto gpuHandle = texMgr->GetSrvHandleGPU(normalized);
				const auto& meta = texMgr->GetMetaData(normalized);

				out.imguiTextureId = reinterpret_cast<void*>(gpuHandle.ptr);
				out.width = static_cast<int>(meta.width);
				out.height = static_cast<int>(meta.height);

				OutputDebugStringA(("[GetImagePreview] OK: " + normalized + "\n").c_str());
				return out;
			};
	}

	// ★ 外側で Begin/End しない。これだけ呼ぶ。
	weaponEditor.DrawImGui(weaponDB, hooks);

	// どうしても lastAppliedID を別窓で出したいなら “別タイトル” で出す
	ImGui::Begin("Weapon Master Debug");
	ImGui::Text("Last Applied ID: %d", lastAppliedID);
	ImGui::End();

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　		ポーズ開始
/// -------------------------------------------------------------
void GamePlayScene::EnterPause()
{
	if (isPaused_) { return; }

	isPaused_ = true;
	if (pauseMenu_)
	{
		pauseMenu_->Open();
	}

	// ポーズ中はカーソルを出してロック解除
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

/// -------------------------------------------------------------
///				　		ポーズ解除
/// -------------------------------------------------------------
void GamePlayScene::ExitPause()
{
	if (!isPaused_) { return; }

	isPaused_ = false;
	if (pauseMenu_)
	{
		pauseMenu_->Close();
	}

	// 復帰時はデバッグカメラ状態に合わせて戻す
	if (input_)
	{
		const bool lock = !isDebugCamera_;
		input_->SetLockCursor(lock);
		input_->SetCursorVisible(!lock);
	}
}

/// -------------------------------------------------------------
///				　		ポーズ中更新
/// -------------------------------------------------------------
void GamePlayScene::UpdatePaused(float deltaTime)
{
	// HUDは更新してOK（値更新・簡易アニメ用）
	if (hudManager_ && characters_.GetPlayer())
	{
		hudManager_->SetHP(characters_.GetPlayer()->GetHP(), characters_.GetPlayer()->GetMaxHP());
		hudManager_->Update(deltaTime);
	}

	if (!pauseMenu_)
	{
		return;
	}

	const PauseMenuCommand cmd = pauseMenu_->Update(input_);

	switch (cmd)
	{
	case PauseMenuCommand::Resume:
		ExitPause();
		break;

	case PauseMenuCommand::ToStageSelect:
		if (input_)
		{
			input_->SetLockCursor(false);
			input_->SetCursorVisible(true);
		}
		sceneManager_->ChangeScene("StageSelectScene");
		break;

	case PauseMenuCommand::ToTitle:
		if (input_)
		{
			input_->SetLockCursor(false);
			input_->SetCursorVisible(true);
		}
		sceneManager_->ChangeScene("TitleScene");
		break;

	case PauseMenuCommand::None:
	default:
		break;
	}
}

void GamePlayScene::UpdateShadowLightViewProjection()
{
	K4E::Vector3 lightDir = shadowLightDirection_;

	K4E::Vector3 managerDir{};
	if (TryGetDirectionalLightFromManager(managerDir))
	{
		lightDir = managerDir;
	}

	lightDir = K4E::Vector3::Normalize(lightDir);

	// プレイヤー中心を影の中心にする
	K4E::Vector3 center = { 0.0f, 0.0f, 0.0f };
	if (auto* player = characters_.GetPlayer())
	{
		if (auto* wt = player->GetWorldTransform())
		{
			center = wt->translate_;
		}
	}

	K4E::Vector3 eye = center - lightDir * shadowDistance_;
	K4E::Vector3 up = { 0.0f, 1.0f, 0.0f };

	// 真上/真下に近いときの保険
	if (std::abs(K4E::Vector3::Dot(lightDir, up)) > 0.99f)
	{
		up = { 0.0f, 0.0f, 1.0f };
	}

	K4E::Matrix4x4 view = K4E::Matrix4x4::MakeLookAtMatrix(eye, center, up);

	// あなたの MakeOrthographicMatrix は
	// (left, top, right, bottom, near, far)
	K4E::Matrix4x4 proj = K4E::Matrix4x4::MakeOrthographicMatrix(
		-shadowOrthoHalfWidth_,
		shadowOrthoHalfHeight_,
		shadowOrthoHalfWidth_,
		-shadowOrthoHalfHeight_,
		shadowNearZ_,
		shadowFarZ_
	);

	shadowLightViewProjection_ = K4E::Matrix4x4::Multiply(view, proj);
}

bool GamePlayScene::TryGetDirectionalLightFromManager(K4E::Vector3& outDirection)
{
	const auto& lights = K4E::LightManager::GetInstance()->GetPunctualLights();

	for (const auto& L : lights)
	{
		if (L.lightType == 1)
		{
			outDirection = K4E::Vector3::Normalize(L.direction);
			return true;
		}
	}
	return false;
}

void GamePlayScene::SetupWaves()
{
	if (!waveManager_) { return; }

	std::vector<WaveDefinition> waves;

	// ------------------------------------------------------------
	// JSON に EnemySpawnPoint がある場合は
	// Blender 側の wave / count を優先して組み立てる
	// ------------------------------------------------------------
	if (!enemySpawnInfos_.empty())
	{
		int maxWave = 0;
		for (const auto& spawn : enemySpawnInfos_)
		{
			if (spawn.wave > maxWave)
			{
				maxWave = spawn.wave;
			}
		}

		if (maxWave <= 0)
		{
			maxWave = 1;
		}

		waves.resize(static_cast<size_t>(maxWave));

		for (int i = 0; i < maxWave; ++i)
		{
			if (i == 0)
			{
				waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 0.0f;
			}
			else if (i == 1)
			{
				waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.0f;
			}
			else
			{
				waves[static_cast<size_t>(i)].delayBeforeSpawnSec = 2.5f;
			}
		}

		for (const auto& spawn : enemySpawnInfos_)
		{
			const int waveIndex = std::max(0, spawn.wave - 1);

			WaveSpawnEntry entry{};
			entry.archetype = EnemyArchetype::RifleGrunt; // いったん固定
			entry.position = spawn.position;

			const int spawnCount = std::max(1, spawn.count);
			for (int i = 0; i < spawnCount; ++i)
			{
				waves[static_cast<size_t>(waveIndex)].enemies.push_back(entry);
			}
		}

		waveManager_->SetWaves(waves);
		return;
	}
}

void GamePlayScene::UpdateIntro(float deltaTime)
{
	if (introCameraPoints_.empty())
	{
		BeginGamePlayFromIntro();
		return;
	}

	if (stage_)
	{
		stage_->Update();
	}

	UpdateShadowLightViewProjection();
	if (stage_)
	{
		stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	}
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	if (skyBox_)
	{
		skyBox_->Update();
	}

	K4E::Camera* camera = K4E::Object3DCommon::GetInstance()->GetDefaultCamera();
	if (!camera)
	{
		BeginGamePlayFromIntro();
		return;
	}

	if (introCameraPoints_.size() == 1)
	{
		const auto& p = introCameraPoints_[0];

		K4E::Vector3 camRot{};
		if (p.aimMode == "Euler")
		{
			camRot = p.rotation;
		}
		else
		{
			const K4E::Vector3 target = FindLookAtPosition(
				introLookAtPoints_,
				p.targetName,
				p.position + K4E::Vector3{ 0.0f, 0.0f, -1.0f });

			camRot = LookAtToEulerRad(p.position, target);
		}

		camera->SetTranslate(p.position);
		camera->SetRotate(camRot);
		camera->SetFovY(p.fov * (std::numbers::pi_v<float> / 180.0f));
		camera->Update();

		introTimer_ -= deltaTime;
		if (introTimer_ <= 0.0f)
		{
			BeginGamePlayFromIntro();
		}
		return;
	}

	if (introCurrentSegment_ >= static_cast<int>(introCameraPoints_.size()) - 1)
	{
		BeginGamePlayFromIntro();
		return;
	}

	const auto& from = introCameraPoints_[introCurrentSegment_];
	const float segmentDuration = std::max(0.01f, from.duration);

	introSegmentTimer_ += deltaTime;

	float t = introSegmentTimer_ / segmentDuration;
	t = std::clamp(t, 0.0f, 1.0f);

	const K4E::Vector3 camPos = EvaluateIntroPosition(
		introCameraPoints_,
		introCurrentSegment_,
		t);

	const K4E::Vector3 camRot = EvaluateIntroCameraRotation(
		introCameraPoints_,
		introLookAtPoints_,
		introCurrentSegment_,
		t,
		camPos);

	const float fovDeg = EvaluateIntroFov(
		introCameraPoints_,
		introCurrentSegment_,
		t);

	camera->SetTranslate(camPos);
	camera->SetRotate(camRot);
	camera->SetFovY(fovDeg * (std::numbers::pi_v<float> / 180.0f));
	camera->Update();

	if (t >= 1.0f)
	{
		introCurrentSegment_++;
		introSegmentTimer_ = 0.0f;

		if (introCurrentSegment_ >= static_cast<int>(introCameraPoints_.size()) - 1)
		{
			BeginGamePlayFromIntro();
		}
	}
}

void GamePlayScene::BeginGamePlayFromIntro()
{
	if (gameFlowState_ != GameFlowState::Intro)
	{
		return;
	}

	gameFlowState_ = GameFlowState::Playing;
	isIntroStarted_ = false;
	introTimer_ = 0.0f;
	introCurrentSegment_ = 0;
	introSegmentTimer_ = 0.0f;

	if (K4E::Camera* camera = K4E::Object3DCommon::GetInstance()->GetDefaultCamera())
	{
		camera->SetRotate({ 0.0f, 0.0f, 0.0f });
		camera->Update();
	}

	if (auto* player = characters_.GetPlayer())
	{
		player->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

		if (hasPlayerSpawnPoint_)
		{
			constexpr float kPlayerSpawnLift = 1.0f;

			K4E::Vector3 spawn = playerSpawnPoint_;
			spawn.y += kPlayerSpawnLift;
			player->SetSpawnPosition(spawn);
		}
	}

	characters_.Update(0.0f);

	if (stage_)
	{
		stage_->Update();
	}

	CollisionUpdate();

	if (input_)
	{
		const bool lock = !isDebugCamera_;
		input_->SetLockCursor(lock);
		input_->SetCursorVisible(!lock);
	}

	if (waveManager_)
	{
		waveManager_->Start();
	}
}

void GamePlayScene::EnterGameClear()
{
	if (gameFlowState_ == GameFlowState::GameClear) { return; }

	// クリアしたら次のステージをアンロック（セーブ）する
	UnlockNextStage();

	gameFlowState_ = GameFlowState::GameClear;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_) { pauseMenu_->Close(); }
	if (resultMenu_) { resultMenu_->Open(ResultMenuMode::GameClear); }

	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

void GamePlayScene::EnterGameOver()
{
	if (gameFlowState_ == GameFlowState::GameOver) { return; }

	gameFlowState_ = GameFlowState::GameOver;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_) { pauseMenu_->Close(); }
	if (resultMenu_) { resultMenu_->Open(ResultMenuMode::GameOver); }

	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

void GamePlayScene::UpdateResult(float deltaTime)
{
	if (resultInputCooldown_ > 0.0f)
	{
		resultInputCooldown_ -= deltaTime;
		if (resultInputCooldown_ < 0.0f)
		{
			resultInputCooldown_ = 0.0f;
		}
	}

	if (!input_ || resultInputCooldown_ > 0.0f)
	{
		return;
	}

	ResultMenuCommand cmd = ResultMenuCommand::None;
	if (resultMenu_)
	{
		cmd = resultMenu_->Update(input_);
	}

	switch (cmd)
	{
	case ResultMenuCommand::NextStage:
		// まだ「次のステージ番号受け渡し」が無いなら、ひとまず StageSelect に飛ばす
		// 後で stageIndex を持たせたらそこに差し替える
		sceneManager_->ChangeScene("StageSelectScene");
		return;

	case ResultMenuCommand::Retry:
		RestartGame();
		return;

	case ResultMenuCommand::ToTitle:
		sceneManager_->ChangeScene("TitleScene");
		return;

	case ResultMenuCommand::None:
	default:
		break;
	}

	// 念のためキーボードも残すなら以下
	if (input_->TriggerKey(DIK_R))
	{
		RestartGame();
		return;
	}
	if (input_->TriggerKey(DIK_T))
	{
		sceneManager_->ChangeScene("TitleScene");
		return;
	}
}

void GamePlayScene::RestartGame()
{
	Finalize();
	Initialize();
}

void GamePlayScene::EnterImGuiFreeze()
{
	if (isImGuiFreeze_) { return; }

	isImGuiFreeze_ = true;

	// もし通常ポーズ中なら解除しておく
	if (isPaused_)
	{
		isPaused_ = false;
		if (pauseMenu_)
		{
			pauseMenu_->Close();
		}
	}

	// ImGui操作しやすいようにカーソル解放
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

void GamePlayScene::ExitImGuiFreeze()
{
	if (!isImGuiFreeze_) { return; }

	isImGuiFreeze_ = false;

	// 通常ゲームに戻る時のカーソル状態
	if (input_)
	{
		const bool lock = !isDebugCamera_;
		input_->SetLockCursor(lock);
		input_->SetCursorVisible(!lock);
	}
}

void GamePlayScene::UpdateImGuiFreeze()
{
	// 完全停止中は何も更新しない
	// 必要なら「解除キーだけ」ここで受けてもよい
}

void GamePlayScene::UnlockNextStage()
{
	auto& repo = StageRepository::GetInstance();

	// ステージセレクトシーンから保存されたステージ一覧を取得
	auto stages = repo.GetStages();
	if (stages.empty()) stages = BuildDefaultStages(); // 万が一のためデフォルトも用意

	// 今クリアしたステージ番号
	const int clearedStageIndex = currentStageIndex_;
	if (clearedStageIndex < 0 || clearedStageIndex >= static_cast<int>(stages.size()))
	{
		repo.SetStages(stages);
		repo.SetStartIndex(0);
		return;
	}

	// 次のステージ番号
	const int nextStageIndex = clearedStageIndex + 1;

	// justUnlockedは一旦全部落とす
	for (auto& stage : stages)
	{
		stage.justUnlocked = false;
	}

	// 範囲内なら次のステージをアンロック
	if (nextStageIndex >= static_cast<int>(stages.size()))
	{
		// 最終ステージなら何もしない
		repo.SetStages(stages); // 一応保存は更新しておく
		repo.SetStartIndex(clearedStageIndex); // 次回も最終ステージから開始
		return;
	}

	// 次のステージをアンロック
	stages[nextStageIndex].locked = false;
	stages[nextStageIndex].justUnlocked = true; // ここだけフラグ立てる

	// 更新されたステージ一覧を保存
	repo.SetStages(stages);

	// 次回開始ステージも更新（これでセレクトに戻ったときアンロック状態が反映される）
	repo.SetStartIndex(nextStageIndex);
}

GamePlayScene::StageAssetPaths GamePlayScene::GetStageAssetPaths(int stageIndex) const
{
	switch (stageIndex)
	{
	case 0: return { "stages/fps_stage00.json", "stages/fps_stage00.gltf" }; // これは最初からアンロックされているステージ
	case 1: return { "stages/fps_stage01.json", "stages/fps_stage01.gltf" }; // 以降はアンロックされるステージ
	case 2: return { "stages/fps_stage02.json", "stages/fps_stage02.gltf" };
	case 3: return { "stages/fps_stage03.json", "stages/fps_stage03.gltf" };
	case 4: return { "stages/fps_stage04.json", "stages/fps_stage04.gltf" };
	default:
		return { "stages/fps_stage00.json", "stages/fps_stage00.gltf" };
	}
}

void GamePlayScene::LoadSpawnPointsFromLevel(const std::string& jsonPath)
{
	enemySpawnInfos_.clear();
	introCameraPoints_.clear();
	introLookAtPoints_.clear();

	hasPlayerSpawnPoint_ = false;
	hasBossSpawnPoint_ = false;

	playerSpawnPoint_ = { 0.0f, 0.0f, 0.0f };
	bossSpawnPoint_ = { 0.0f, 0.0f, 0.0f };

	const std::unique_ptr<K4E::LevelData> levelData = K4E::LevelLoader::LoadLevel(jsonPath);
	if (!levelData)
	{
		return;
	}

	for (const auto& object : levelData->objects)
	{
		if (IsPlayerSpawnType(object.type))
		{
			if (!hasPlayerSpawnPoint_)
			{
				playerSpawnPoint_ = object.position;
				hasPlayerSpawnPoint_ = true;
			}
		}
		else if (IsEnemySpawnType(object.type))
		{
			EnemySpawnInfo info{};
			info.position = object.position;

			if (object.hasSpawnProps)
			{
				info.wave = (object.spawnProps.wave > 0) ? object.spawnProps.wave : 1;
				info.group = object.spawnProps.group;
				info.count = (object.spawnProps.count > 0) ? object.spawnProps.count : 1;
			}

			enemySpawnInfos_.push_back(info);
		}
		else if (IsBossSpawnType(object.type))
		{
			if (!hasBossSpawnPoint_)
			{
				bossSpawnPoint_ = object.position;
				hasBossSpawnPoint_ = true;
			}
		}
		else if (object.type == "IntroCameraPoint")
		{
			IntroCameraPointInfo info{};
			info.position = object.position;
			info.rotation = object.rotation;

			if (object.hasIntroCameraProps)
			{
				info.order = object.introCameraProps.order;
				info.duration = object.introCameraProps.duration;
				info.fov = object.introCameraProps.fov;
				info.targetName = object.introCameraProps.targetName;
				info.interpMode = object.introCameraProps.interpMode;
				info.aimMode = object.introCameraProps.aimMode;
			}

			introCameraPoints_.push_back(info);
		}
		else if (object.type == "IntroLookAtPoint")
		{
			IntroLookAtPointInfo info{};
			info.name = object.name;
			info.position = object.position;
			introLookAtPoints_.push_back(info);
		}
	}

	std::sort(introCameraPoints_.begin(), introCameraPoints_.end(),
		[](const IntroCameraPointInfo& a, const IntroCameraPointInfo& b)
		{
			return a.order < b.order;
		});
}

/// -------------------------------------------------------------
///				　			Debug用更新処理
/// -------------------------------------------------------------
void GamePlayScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		K4E::Object3DCommon::GetInstance()->SetDebugCamera(!K4E::Object3DCommon::GetInstance()->GetDebugCamera());
		K4E::Wireframe::GetInstance()->SetDebugCamera(!K4E::Wireframe::GetInstance()->GetDebugCamera());
		//K4E::ParticleManager::GetInstance()->SetDebugCamera(!K4E::ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		isDebugCamera_ = !isDebugCamera_;

		characters_.GetPlayer()->SetDebugCamera(isDebugCamera_);

		// カーソルのロックと表示を切り替える
		input_->SetLockCursor(!isDebugCamera_);
		input_->SetCursorVisible(isDebugCamera_);
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　		衝突判定更新処理
/// -------------------------------------------------------------
void GamePlayScene::CollisionUpdate()
{
	if (!collisionManager_) return;
	collisionManager_->Update();
	collisionManager_->CheckAllCollisions();
}
