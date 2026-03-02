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

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#include "Player.h"

#include "WeaponMasterDataDatabase.h"
#include "WeaponMasterDataEditor.h"
#include "WeaponMasterDataWriter.h"
#include <filesystem>

using namespace Ken4lowEngine;

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

	stage_ = std::make_unique<K4E::Stage>();
	stage_->Initialize("stages/fps_stage00.json", "fps_stage00.gltf");
	stage_->RegisterColliders(collisionManager_.get());

	EnemyBase::SetGlobalStageWorldAABBs(&stage_->GetWorldAABBs());

	auto player = characters_.GetPlayer();
	player->SetStageWorldAABBs(&stage_->GetWorldAABBs());

	// Playerの衝突サイズを設定
	WorldCollisionSettings playerCollisionSettings{};
	playerCollisionSettings.half = { 0.5f, 1.0f, 0.5f }; // プレイヤーの半サイズ
	playerCollisionSettings.centerOffset = { 0.0f, 1.0f, 0.0f };			 // プレイヤーの見た目座標と物理中心の差
	player->SetWorldCollisionSettings(playerCollisionSettings);

	isPaused_ = false;
	gameFlowState_ = GameFlowState::Playing;
	resultInputCooldown_ = 0.0f;

	waveManager_ = std::make_unique<WaveManager>();
	SetupWaves();
	waveManager_->Start();

	prevWaveNumber_ = 0;
	prevWaveInProgress_ = false;
	prevAllWavesCleared_ = false;
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// デルタタイムの取得
	const float deltaTime = dxCommon_->GetFPSCounter().GetDeltaTime();

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
		return; // トグルしたフレームはここで終了（誤操作防止）
	}

	// ポーズ中はゲーム進行を止める（メニューだけ更新）
	if (isPaused_)
	{
		UpdatePaused(deltaTime);
		return;
	}

	// デバッグカメラの更新
	UpdateDebug();

	// キャラクター関連の更新
	characters_.Update(deltaTime);

	// 影用のライトのビュー射影行列を更新してキャラクターとステージにセット
	UpdateShadowLightViewProjection();
	stage_->UpdateShadowMatrix(shadowLightViewProjection_);
	characters_.UpdateShadowMatrix(shadowLightViewProjection_);

	// 弾丸マネージャーの更新
	if (bulletManager_)
	{
		bulletManager_->Update(deltaTime);
	}

	// 衝突判定の更新
	CollisionUpdate();

	if (skyBox_) skyBox_->Update();

	// HUD更新（通常時）
	if (hudManager_ && characters_.GetPlayer())
	{
		hudManager_->SetHP(characters_.GetPlayer()->GetHP(), characters_.GetPlayer()->GetMaxHP());
		hudManager_->Update(deltaTime);
	}

	// ステージの更新
	if (stage_) { stage_->Update(); }

	// ------------------------------------------------------------
	// プレイヤー死亡判定
	// ------------------------------------------------------------
	if (characters_.GetPlayer() && characters_.GetPlayer()->GetHP() <= 0)
	{
		EnterGameOver();
		return;
	}

	// ------------------------------------------------------------
	// ウェーブ進行
	// ------------------------------------------------------------
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

			// ウェーブ開始時に1回だけ通知
			if (isWaveInProgress && (!prevWaveInProgress_ || currentWave != prevWaveNumber_))
			{
				hudManager_->NotifyWaveStarted(currentWave, isFinalWave);
			}

			// 全ウェーブクリア時に1回だけ通知
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

	// キャラクターの描画
	characters_.Draw();

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

#endif // _DEBUG

	// ワイヤーフレームの描画
	K4E::Wireframe::GetInstance()->DrawGrid(200.0f, 50.0f, { 0.25f, 0.25f, 0.25f,1.0f });
}

void GamePlayScene::DrawShadowObjects()
{
	if (stage_) { stage_->DrawShadow(); } // もし Stage 側にあるなら
	characters_.DrawShadow();
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

	if (hudManager_) { hudManager_->Draw(); }
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

	// =========================================================
	// Wave 1
	// 広めに散らして、最初から左右を少し意識させる
	// =========================================================
	{
		WaveDefinition wave;
		wave.delayBeforeSpawnSec = 0.0f;
		wave.enemies =
		{
			// 前線
			{ EnemyArchetype::RifleGrunt, { -10.0f, 3.0f, 26.0f } },
			{ EnemyArchetype::RifleGrunt, {  10.0f, 3.0f, 26.0f } },

			// 中央圧
			{ EnemyArchetype::SMGFlanker, {   0.0f, 3.0f, 20.0f } },
			// 少し後ろ
			{ EnemyArchetype::SMGFlanker, { -18.0f, 3.0f, 34.0f } },
			{ EnemyArchetype::SMGFlanker, {  18.0f, 3.0f, 34.0f } },
		};
		waves.push_back(wave);
	}

	// =========================================================
	// Wave 2
	// 左右フランクと中央押し込みを強める
	// =========================================================
	{
		WaveDefinition wave;
		wave.delayBeforeSpawnSec = 2.0f;
		wave.enemies =
		{
			// 前寄り
			{ EnemyArchetype::RifleGrunt, {  -6.0f, 2.0f, 22.0f } },
			{ EnemyArchetype::RifleGrunt, {   6.0f, 2.0f, 22.0f } },

			// 左右フランク
			{ EnemyArchetype::SMGFlanker, { -22.0f, 2.0f, 18.0f } },
			{ EnemyArchetype::SMGFlanker, {  22.0f, 2.0f, 18.0f } },

			// 中央奥
			{ EnemyArchetype::RifleGrunt, {   0.0f, 2.0f, 32.0f } },

			// 後方左右
			{ EnemyArchetype::SMGFlanker, { -16.0f, 2.0f, 40.0f } },
			{ EnemyArchetype::SMGFlanker, {  16.0f, 2.0f, 40.0f } },
		};
		waves.push_back(wave);
	}

	// =========================================================
	// Wave 3
	// 最終ウェーブ。高所 + 地上ラッシュ
	// ※ 高さはステージに高台がある前提。無ければ Y=2.0f に戻す
	// =========================================================
	{
		WaveDefinition wave;
		wave.delayBeforeSpawnSec = 2.5f;
		wave.enemies =
		{
			// 高所スナイパー
			{ EnemyArchetype::Sniper,     { -15.0f, 14.0f, 35.0f } },
			{ EnemyArchetype::Sniper,     {  15.0f, 14.0f, 35.0f } },

			// 前線制圧
			{ EnemyArchetype::RifleGrunt, { -10.0f, 5.0f, 0.0f } },
			{ EnemyArchetype::RifleGrunt, {   0.0f, 5.0f, 0.0f } },
			{ EnemyArchetype::RifleGrunt, {  10.0f, 5.0f, 0.0f } },

			// 左右から詰める
			{ EnemyArchetype::SMGFlanker, { -24.0f, 2.0f, 2.0f } },
			{ EnemyArchetype::SMGFlanker, {  24.0f, 2.0f, 2.0f } },

			// 中央奥の追撃
			{ EnemyArchetype::RifleGrunt, { -14.0f, 2.0f, 38.0f } },
			{ EnemyArchetype::RifleGrunt, {  14.0f, 2.0f, 38.0f } },

			// 裏気味の圧
			{ EnemyArchetype::SMGFlanker, {   0.0f, 2.0f, 40.0f } },
		};
		waves.push_back(wave);
	}

	waveManager_->SetWaves(waves);
}

void GamePlayScene::EnterGameClear()
{
	if (gameFlowState_ == GameFlowState::GameClear) { return; }

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
