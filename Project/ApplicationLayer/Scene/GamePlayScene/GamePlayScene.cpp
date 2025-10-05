#define NOMINMAX
#include "GamePlayScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <SpriteManager.h>
#include "Object3DCommon.h"
#include <AnimationPipelineBuilder.h>
#include "SkyBoxManager.h"
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "Wireframe.h"
#include "AudioManager.h"
#include <SceneManager.h>
#include <ScoreManager.h>
#include "LevelLoader.h"
#include "EnemyBullet.h"

#ifdef _DEBUG
#include <DebugCamera.h>
#endif // _DEBUG


/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
#ifdef _DEBUG
	// デバッグカメラの初期化
	DebugCamera::GetInstance()->Initialize();
#endif // _DEBUG

	input_ = Input::GetInstance();

	// カーソルをロック
	Input::GetInstance()->SetLockCursor(true);
	ShowCursor(false);// 表示・非表示も連動（オプション）

	EnemyBullet::ClearAll();

	// スコアの初期化
	ScoreManager::GetInstance()->Initialize();

	// プレイヤーの生成と初期化
	player_ = std::make_unique<Player>();
	player_->Initialize();

	enemies_.clear();
	enemies_.reserve(32); // 敵の数を予約
	spawnRequests_.clear();

	// ★これまでの3点push_backを削除して↓に置き換え
	enemySpawnPoints_.clear();
	RebuildSpawnPointsAroundPlayer(/*count=*/8, spawnRadiusMin_, spawnRadiusMax_);

	enemySpawnPoints_.clear(); // 重複防止
	{
		const Vector3 base = player_->GetAnimationModel()->GetTranslate() + Vector3(0.0f, 1.0f, 0.0f);
		enemySpawnPoints_.push_back(base + Vector3{ -20.0f,0.0f,20.0f });
		enemySpawnPoints_.push_back(base + Vector3{ 20.0f,0.0f,20.0f });
		enemySpawnPoints_.push_back(base + Vector3{ 0.0f,0.0f,18.0f });
	}

	// ★ ウェーブ構成 → 開始
	InitWaves();        // Wave の total/batch/interval を定義（あなたの下部関数を使用）
	waveIndex_ = 0;
	BeginWave(waveIndex_);  // HUD更新もここで入ります

	// ボス
	boss_.reset(); // 念のためリセット
	bossSpawned_ = false; // リセット

	// 追従カメラの生成と初期化
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(player_.get());
	fpsCamera_->SetDeltaTime(player_->GetDeltaTime());

	// プレイヤーにカメラを設定
	player_->SetCamera(fpsCamera_->GetCamera());
	player_->SetFpsCamera(fpsCamera_.get());

	// クロスヘアの生成と初期化
	crosshair_ = std::make_unique<Crosshair>();
	crosshair_->Initialize();

	player_->SetCrosshair(crosshair_.get()); // プレイヤーにクロスヘアを設定

	// 衝突マネージャの生成
	collisionManager_ = std::make_unique<CollisionManager>();
	collisionManager_->Initialize();

	// HUDマネージャーの生成と初期化
	hudManager_ = std::make_unique<HUDManager>();
	hudManager_->Initialize();
	hudManager_->SyncWeaponSlots(2);     // ★ 今は2本
	hudManager_->SetActiveWeaponIndex(0); // ★ 初期は1番目

	// 結果マネージャーの生成と初期化
	resultManager_ = std::make_unique<ResultManager>();
	resultManager_->Initialize();

	// 初期化内に追加（プレイヤー近くに1個スポーン）
	itemManager_ = std::make_unique<ItemManager>();
	itemManager_->Initialize();

	terrein_ = std::make_unique<Object3D>();
	// 地形オブジェクトの初期化
	terrein_->Initialize("terrain.gltf");
	terrein_->SetScale({ 50.0f, 50.0f, 50.0f });

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// デバッグカメラの更新
	UpdateDebug();

	// --- ポーズトグル（ESCキーでON/OFF） ---
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

		if (gameState_ == GameState::Playing)
		{
			isPaused_ = true; // ポーズ状態にする
			gameState_ = GameState::Paused;
			Input::GetInstance()->SetLockCursor(false);
			ShowCursor(true);
		}
		else if (gameState_ == GameState::Paused)
		{
			isPaused_ = false; // ポーズ解除
			gameState_ = GameState::Playing;
			Input::GetInstance()->SetLockCursor(true);
			ShowCursor(false);
		}
	}

	switch (gameState_)
	{
	case GameState::Playing:

		UpdatePlaying();

		break;

	case GameState::Paused:

		UpdatePaused();
		// 何も動かさない（またはポーズUIだけ更新）
		break;

	case GameState::Result:

		UpdateResult();

		break;
	}

	terrein_->Update();

	skyBox_->Update();

	EnemyBullet::UpdateAll(player_.get(), player_->GetDeltaTime());

}


/// -------------------------------------------------------------
///				　		3Dオブジェクトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
#pragma region スカイボックスの描画

	// スカイボックスの共通描画設定
	SkyBoxManager::GetInstance()->SetRenderSetting();

	skyBox_->Draw();

#pragma endregion


#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

	terrein_->Draw();

	// アイテムの描画
	itemManager_->Draw();

	for (const auto& enemy : enemies_) enemy->Draw();

	EnemyBullet::DrawAll();

	// プレイヤーの描画
	player_->Draw();

#pragma endregion


#pragma region アニメーションモデルの描画

	// アニメーションモデルの共通描画設定
	AnimationPipelineBuilder::GetInstance()->SetRenderSetting();

	if (boss_) boss_->Draw();


#pragma endregion


#ifdef _DEBUG
	// 衝突判定を行うオブジェクトの描画
	collisionManager_->Draw();

	// FPSカメラの描画
	fpsCamera_->DrawDebugCamera();

	// ワイヤーフレームの描画
	//Wireframe::GetInstance()->DrawGrid(100.0f, 100.0f, { 0.25f, 0.25f, 0.25f,1.0f });

#endif // _DEBUG
}


/// -------------------------------------------------------------
///				　		2Dスプライトの描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
#pragma region スプライトの描画                    

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

#pragma endregion


#pragma region UIの描画

	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// クロスヘアの描画
	crosshair_->Draw();

	// HUDマネージャーの描画
	hudManager_->Draw();

	if (gameState_ == GameState::Result) resultManager_->Draw();

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{

}


/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	// ライト
	LightManager::GetInstance()->DrawImGui();

	// プレイヤー
	player_->DrawImGui();

	if (boss_) boss_->DrawImGui();

	terrein_->DrawImGui();
}


void GamePlayScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F12))
	{
		if (isPaused_) return; // ポーズ中はデバッグカメラ切り替えを無効化

		Object3DCommon::GetInstance()->SetDebugCamera(!Object3DCommon::GetInstance()->GetDebugCamera());
		Wireframe::GetInstance()->SetDebugCamera(!Wireframe::GetInstance()->GetDebugCamera());
		ParticleManager::GetInstance()->SetDebugCamera(!ParticleManager::GetInstance()->GetDebugCamera());
		skyBox_->SetDebugCamera(!skyBox_->GetDebugCamera());
		player_->SetDebugCamera(!player_->IsDebugCamera());
		isDebugCamera_ = !isDebugCamera_;
		Input::GetInstance()->SetLockCursor(isDebugCamera_);
		ShowCursor(!isDebugCamera_);// 表示・非表示も連動（オプション）
	}
#endif // _DEBUG
}

/// -------------------------------------------------------------
///				　			衝突判定と応答
/// -------------------------------------------------------------
void GamePlayScene::CheckAllCollisions()
{
	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// コライダーをリストに登録
	player_->RegisterColliders(collisionManager_.get()); // プレイヤーのコライダーを登録
	if (boss_) boss_->RegisterColliders(collisionManager_.get());

	for (auto& enemy : enemies_)
	{
		if (enemy && !enemy->IsDead()) collisionManager_->AddCollider(enemy.get());
	}

	// プレイヤーの弾の登録
	for (const auto& bullet : player_->GetBullets())
	{
		if (!bullet->IsDead()) collisionManager_->AddCollider(bullet.get());
	}

	// プレイヤーが死亡している場合、コライダーを削除
	if (player_->IsDead()) collisionManager_->RemoveCollider(player_.get());

	// アイテムのコライダーを登録
	itemManager_->RegisterColliders(collisionManager_.get());

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}

void GamePlayScene::UpdatePlaying()
{
	UpdateWaveSpawner(player_->GetDeltaTime()); // ウェーブスポーナーの更新

	// プレイヤーの死亡判定 -> リザルト画面へ移行
	if (player_->IsDead())
	{
		gameState_ = GameState::Result;
		Input::GetInstance()->SetLockCursor(false);
		ShowCursor(true);

		// ★ 結果情報を設定
		resultManager_->SetFinalScore(ScoreManager::GetInstance()->GetScore());
		resultManager_->SetKillCount(ScoreManager::GetInstance()->GetKills());
	}

	// リロード演出（既存）
	if (Weapon* weapon = player_->GetCurrentWeapon())
	{
		if (weapon->IsReloading())
		{
			crosshair_->SetVisible(false);
			hudManager_->SetReloading(true, weapon->GetReloadProgress());
		}
		else
		{
			crosshair_->SetVisible(true);
			hudManager_->SetReloading(false, 0.0f);
		}
	}

	player_->Update();
	if (boss_) boss_->Update();
	for (auto& enemy : enemies_) enemy->Update();

	// ボスが死んだらリザルト画面へ移行
	if (boss_ && boss_->IsDead())
	{
		gameState_ = GameState::Result;
		Input::GetInstance()->SetLockCursor(false);
		ShowCursor(true);
		// 結果情報を設定
		resultManager_->SetFinalScore(ScoreManager::GetInstance()->GetScore());
		resultManager_->SetKillCount(ScoreManager::GetInstance()->GetKills());
	}

	hudManager_->Update();
	hudManager_->SetAmmoFromWeapon(player_->GetCurrentWeapon());// 新しいコード（武器の情報を直接渡す）
	hudManager_->SetHP(player_->GetHP(), player_->GetMaxHP());
	hudManager_->SetWeapon(player_->GetCurrentWeapon());

	if (input_->TriggerKey(DIK_1)) hudManager_->SetActiveWeaponIndex(0);
	if (input_->TriggerKey(DIK_2)) hudManager_->SetActiveWeaponIndex(1);
	if (input_->TriggerKey(DIK_3)) hudManager_->SetActiveWeaponIndex(2);
	if (input_->TriggerKey(DIK_4)) hudManager_->SetActiveWeaponIndex(3);
	if (input_->TriggerKey(DIK_5)) hudManager_->SetActiveWeaponIndex(4);
	if (input_->TriggerKey(DIK_6)) hudManager_->SetActiveWeaponIndex(5);

	// 取得例：プレイヤー→現在の武器→WeaponType をもらう
	if (auto wp = player_->GetCurrentWeapon()) {
		switch (wp->GetWeaponType()) {
		case WeaponType::Primary:  hudManager_->SetActiveWeaponCategory(WeaponCategory::Primary);  break;
		case WeaponType::Backup:   hudManager_->SetActiveWeaponCategory(WeaponCategory::Backup);   break;
		case WeaponType::Melee:    hudManager_->SetActiveWeaponCategory(WeaponCategory::Melee);    break;
		case WeaponType::Special:  hudManager_->SetActiveWeaponCategory(WeaponCategory::Special);  break;
		case WeaponType::Sniper:   hudManager_->SetActiveWeaponCategory(WeaponCategory::Sniper);   break;
		case WeaponType::Heavy:    hudManager_->SetActiveWeaponCategory(WeaponCategory::Heavy);    break;
		default:                   hudManager_->SetActiveWeaponCategory(WeaponCategory::Unknown);  break;
		}
	}

	bool canFire = false;
	bool isADS = player_->GetController()->IsAimingInput();
	bool isReloading = player_->GetCurrentWeapon()->IsReloading();

	hudManager_->SyncActionStates(canFire, isADS, isReloading);

	fpsCamera_->Update();
	crosshair_->Update();

	CheckAllCollisions();

	itemManager_->Update(player_.get());
}

void GamePlayScene::UpdatePaused()
{
#ifdef _DEBUG
	// 衝突判定
	collisionManager_->Update();
#endif // _DEBUG

	fpsCamera_->Update(true); // 回転行列だけ更新するなら true を渡す
}

void GamePlayScene::UpdateResult()
{
	resultManager_->Update();
	if (resultManager_->IsRestartRequested())
	{
		sceneManager_->ChangeScene("GamePlayScene");
	}
	if (resultManager_->IsReturnToTitleRequested())
	{
		sceneManager_->ChangeScene("TitleScene");
	}
}

void GamePlayScene::InitWaves()
{
	waves_.clear();
	// total, batchSize(=4固定), spawnInterval(未使用化), batchDelay(未使用化)
	waves_.push_back({ 4, 2, 0.0f, 0.0f }); // 4体×4バッチ
	waves_.push_back({ 16, 4, 0.0f, 0.0f });
	//waves_.push_back({ 20, 4, 0.0f, 0.0f }); // 4体×5バッチ
}

void GamePlayScene::BeginWave(size_t idx)
{
	const auto& w = waves_[idx];
	enemiesToSpawn_ = w.totalEnemies;
	aliveEnemies_ = CountAliveEnemies(); // 念のため
	spawnTimer_ = 0.0f;

	// バッチ数と端数を求める
	batchLeftInThisWave_ = w.totalEnemies / w.batchSize;
	batchRemainder_ = w.totalEnemies % w.batchSize;
	batchCooldown_ = 0.0f;

	spawnedInThisBatch_ = 0; // 毎ウェーブ初期化

	// HUDへ反映
	UpdateHudWaveInfo();
}

void GamePlayScene::UpdateWaveSpawner(float dt)
{
	if (waveIndex_ >= waves_.size()) return;

	const auto& w = waves_[waveIndex_];

	// ★ いま生きてる敵が0なら、次のバッチを一斉スポーン
	if (CountAliveEnemies() == 0) {
		// まだこのウェーブで出す敵が残っている？
		if (enemiesToSpawn_ > 0) {
			// プレイヤーの現在位置を中心にスポーン点を組み直すと“周囲から湧く”になる
			RebuildSpawnPointsAroundPlayer(8, spawnRadiusMin_, spawnRadiusMax_);

			int n = std::min(w.batchSize, enemiesToSpawn_);
			for (int i = 0; i < n; ++i) {
				const Vector3& p = enemySpawnPoints_[rand() % enemySpawnPoints_.size()];
				SpawnOneEnemy(p);
			}
			enemiesToSpawn_ -= n;
			UpdateHudWaveInfo();
			return;
		}

		// もうこのウェーブ分を全部出し切り、かつ全滅＝ウェーブ完了
		if (enemiesToSpawn_ <= 0) {
			++waveIndex_;
			if (waveIndex_ < waves_.size()) {
				BeginWave(waveIndex_);
			}
			else {
				// 全ウェーブ終了→ボス出現（既存ロジック）
				if (!bossSpawned_) {
					boss_ = std::make_unique<Boss>();
					boss_->Initialize();
					boss_->SetPlayer(player_.get());
					bossSpawned_ = true;
				}
			}
		}
	}

	// HUD更新
	UpdateHudWaveInfo();
}

void GamePlayScene::SpawnOneEnemy(const Vector3& pos)
{
	auto enemy = std::make_unique<Enemy>();
	enemy->Initialize(player_.get(), pos);
	enemy->SetItemManager(itemManager_.get());
	enemies_.push_back(std::move(enemy));
}

int GamePlayScene::CountAliveEnemies() const
{
	int alive = 0;
	for (auto& e : enemies_) if (e && !e->IsDead()) ++alive;
	return alive;
}

void GamePlayScene::UpdateHudWaveInfo()
{
	if (hudManager_)
	{
		hudManager_->SetWaveInfo((int)waveIndex_ + 1, (int)waves_.size());
		const int remaining = enemiesToSpawn_ + CountAliveEnemies();
		hudManager_->SetEnemiesRemaining(remaining);
	}
}

Vector3 GamePlayScene::RandomSpawnPointAroundPlayer(float minR, float maxR)
{
	float ang = (float)rand() / RAND_MAX * std::numbers::pi_v<float> *2.0f; // 0..2π
	float t = (float)rand() / RAND_MAX;
	float r = minR + (maxR - minR) * t;
	Vector3 base = player_->GetAnimationModel()->GetTranslate();
	return base + Vector3{ std::cos(ang) * r, 0.0f, std::sin(ang) * r };
}

void GamePlayScene::RebuildSpawnPointsAroundPlayer(int count, float minR, float maxR)
{
	enemySpawnPoints_.clear();
	Vector3 base = player_->GetAnimationModel()->GetTranslate();
	for (int i = 0; i < count; ++i) {
		float ang = (std::numbers::pi_v<float>*2 * i) / count;
		float r = (minR + maxR) * 0.5f;
		// ほんの少しランダムを足す
		r += ((float)rand() / RAND_MAX - 0.5f) * (maxR - minR) * 0.25f;
		enemySpawnPoints_.push_back(base + Vector3{ std::cos(ang) * r, 0.0f, std::sin(ang) * r });
	}
}
