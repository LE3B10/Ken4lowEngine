#define NOMINMAX
#include "GamePlayingState.h"
#include "GamePlayScene.h"
#include "StageRepository.h"
#include <LevelLoader.h>
#include <GameOverState.h>
#include <GameClearState.h>
#include "PauseOverlay.h"
#include "GamePauseState.h"
#include <SceneManager.h>

#include "Input.h"
#include "Player.h"

#include <vector>
#include <memory>

void GamePlayingState::Enter(GamePlayScene* scene)
{
	if (!scene) return;

	using State = GamePlayScene::State;
	scene->SetState(State::Playing);

	auto* input = scene->GetInput();
	input->SetLockCursor(true);
	ShowCursor(false);

	// --- ここから下は「ゲームプレイ初回だけ」走らせたい処理 ---
	if (scene->IsGameplayInitialized()) {
		// すでにゲーム進行中だった場合 → 何も初期化しないで戻る
		return;
	}

	scene->SetGameplayInitialized(true);

	// Wave 情報をシーンから取得
	auto& waveConfigs = *scene->GetWaveConfigs();

	// シーン側の状態を初期化
	scene->SetCurrentWaveIndex(0);
	scene->SetAllWavesCleared(false);
	scene->SetBossSpawned(false);

	// 鍵フラグもリセット
	scene->SetNextStageKeySpawned(false);

	// 敵 / ボスもクリア
	auto& enemies = scene->GetEnemies();
	enemies.clear();
	scene->GetBoss().reset();

	// 最初の Wave を出す
	if (!waveConfigs.empty()) {
		SpawnWave(scene, 0);
	}
}

void GamePlayingState::Update(GamePlayScene* scene, float deltaTime)
{
	// シーンが有効か確認
	if (!scene) return;

	using State = GamePlayScene::State;

	auto* input = scene->GetInput();

	// ---------- ESC キーでポーズへ遷移 ----------
	if (input->TriggerKey(DIK_ESCAPE))
	{
		// デバッグカメラ中はポーズ禁止
		if (scene->IsDebugCamera()) {
			return;
		}

		// カーソル解放
		input->SetLockCursor(false);
		ShowCursor(true);

		// ポーズオーバーレイ生成（まだ無ければ）
		if (!scene->GetPauseOverlay())
		{
			auto overlay = std::make_unique<PauseOverlay>();
			overlay->Open(SceneManager::GetInstance());
			scene->SetPauseOverlay(std::move(overlay));
		}

		// 状態を Paused にしてステートを切り替え
		scene->SetPaused(true);
		scene->SetState(State::Paused);
		scene->ChangeState(std::make_unique<GamePauseState>());

		return; // このフレームはここで終わり
	}

	using State = GamePlayScene::State;

	auto* player = scene->GetPlayer();
	std::vector<std::unique_ptr<Enemy>>& enemies = scene->GetEnemies();
	auto& boss = scene->GetBoss(); // ボス敵
	auto* skyBox = scene->GetSkyBox();
	auto* crosshair = scene->GetCrosshair();
	auto* itemManager = scene->GetItemManager();
	auto& normalDropTable = scene->GetItemDropTable();
	auto& levelObjectManager = scene->GetLevelObjectManager();

	bool bossSpawned = scene->IsBossSpawned();
	int currentWaveIndex = scene->GetCurrentWaveIndex();
	auto& waveConfigs = *scene->GetWaveConfigs();

	player->Update(deltaTime);

	for (auto& e : enemies) {
		e->Update(deltaTime);
	}

	if (boss) {
		boss->Update(deltaTime);
	}

	skyBox->Update();
	crosshair->Update();

	// プレイヤーが死亡したらゲームオーバーへ移行
	if (player->IsDeadNow())
	{
		input->SetLockCursor(false);
		ShowCursor(true);

		// ゲームオーバーへ移行
		scene->SetState(State::GameOver);
		scene->ChangeState(std::make_unique<GameOverState>());
	}

	// 敵の死亡処理
	for (auto& e : enemies) {
		if (e->IsDeadNow()) {
			ItemType drop;
			if (normalDropTable.RollForDrop(drop)) {
				const Vector3 pos = e->GetDropPosAtDeath() - Vector3(0.0f, 2.0f, 0.0f);
				itemManager->Spawn(drop, pos);
			}
		}
	}

	// アイテム更新
	itemManager->Update(player, deltaTime);

	// レベルオブジェクト更新
	levelObjectManager->Update();

	// 通常敵の死亡したやつを消す
	enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const std::unique_ptr<Enemy>& e) {
		return e->IsDeadNow(); }), enemies.end());

	// ウェーブクリア / ボス撃破判定
	bool hasAliveEnemies = !enemies.empty();

	// ボスがいるならそれもチェック
	if (!bossSpawned)
	{
		if (!hasAliveEnemies)
		{
			if (currentWaveIndex + 1 < static_cast<int>(waveConfigs.size()))
			{
				currentWaveIndex++;
				scene->SetCurrentWaveIndex(currentWaveIndex);
				SpawnWave(scene, currentWaveIndex);
			}
			else
			{
				scene->SetAllWavesCleared(true);
				SpawnBoss(scene);
			}
		}
	}
	else
	{
		// ボスが存在していて、かつ「死亡演出まで完了」しているか？
		if (boss && boss->IsDeadNow())
		{
			// 鍵を1回だけスポーン（まだ出していないときだけ）
			if (!scene->IsNextStageKeySpawned())
			{
				itemManager->Spawn(ItemType::NextStageKey, boss->GetCenterPosition());
				itemManager->Update(player, deltaTime); // 即座に更新して出現させておく
				scene->SetNextStageKeySpawned(true);
			}

			// 鍵を拾ったらステージクリア扱いにする
			if (itemManager->ConsumeCollected(ItemType::NextStageKey))
			{
				GoToNextStage(scene);
			}
		}
	}

	// 衝突判定チェック
	auto collisionManager = scene->GetCollisionManager();
	collisionManager->Update();
	CheckCollisions(scene);
}

void GamePlayingState::Draw3DObjects(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;

	auto* player_ = scene->GetPlayer();
	auto& enemies_ = scene->GetEnemies();
	auto* itemManager_ = scene->GetItemManager();
	auto& levelObjectManager_ = scene->GetLevelObjectManager();
	auto& boss_ = scene->GetBoss();

	player_->Draw();

	for (auto& e : enemies_) {
		e->Draw();
	}

	if (boss_) {
		boss_->Draw();
	}

	itemManager_->Draw();

	levelObjectManager_->Draw();
}

void GamePlayingState::Draw2DSprites(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;
	auto* crosshair = scene->GetCrosshair();
	crosshair->Draw();
}

void GamePlayingState::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}

void GamePlayingState::SpawnWave(GamePlayScene* scene, int waveIndex)
{
	auto& waveConfigs = *scene->GetWaveConfigs();
	auto* player = scene->GetPlayer();
	std::vector<std::unique_ptr<Enemy>>& enemies = scene->GetEnemies();
	auto& levelObjectManager = scene->GetLevelObjectManager();
	auto& currentStageConfig = scene->GetCurrentWaveConfig();

	if (waveIndex < 0 || waveIndex >= static_cast<int>(waveConfigs.size())) return;

	const auto& cfg = waveConfigs[waveIndex];

	enemies.clear();

	// ステージ側で決める基準位置
	const Vector3 center = currentStageConfig.bossSpawnPos;

	for (int i = 0; i < cfg.enemyCount; ++i)
	{
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetPlayerPointer(player);
		enemy->SetLevelObjectManager(levelObjectManager.get());

		// ステージ依存パラメータを適用
		enemy->ApplyStageParams(
			currentStageConfig.enemymaxHp,
			currentStageConfig.enemyWalkSpeed,
			currentStageConfig.enemyChaseSpeed,
			currentStageConfig.enemyAttackDamage,
			currentStageConfig.enemyAttackCooldown,
			currentStageConfig.enemyDetectRadius
		);

		Vector3 pos;

		// 個別指定があればそれを使う
		if (!cfg.spawnPositions.empty() && i < (int)cfg.spawnPositions.size())
		{
			pos = cfg.spawnPositions[i];
		}
		else
		{
			// なければ「ステージ基準位置」を中心に円形スポーン
			const float angle =
				(2.0f * std::numbers::pi_v<float> / std::max(cfg.enemyCount, 1))
				* i;

			pos = center + Vector3{
				std::cos(angle) * cfg.spawnRadius,
				0.0f,                               // 敵の足元Y
				std::sin(angle) * cfg.spawnRadius
			};
		}

		enemy->SetSpawnPosition(pos);

		enemies.push_back(std::move(enemy));
	}
}

void GamePlayingState::SpawnBoss(GamePlayScene* scene)
{
	auto& boss = scene->GetBoss();

	if (scene->IsBossSpawned()) return;
	scene->SetBossSpawned(true);

	boss = std::make_unique<BossEnemy>();
	boss->Initialize();
	boss->SetLevelObjectManager(scene->GetLevelObjectManager().get());
	boss->SetPlayer(scene->GetPlayer());
	boss->Update(0.0f); // 一度更新しておく
}

void GamePlayingState::OnStageClear(GamePlayScene* scene)
{
	using State = GamePlayScene::State;

	float gameClearTimer = scene->GetGameClearTimer();
	bool gameClearInputAccepted = scene->IsGameClearInputAccepted();

	// 状態を GameClear に
	scene->SetState(State::GameClear);
	scene->ChangeState(std::make_unique<GameClearState>()); // ステート切り替え

	// クリア演出用タイマー初期化
	gameClearTimer = 0.0f;
	gameClearInputAccepted = false;

	// シーンに書き戻す
	scene->SetGameClearTimer(gameClearTimer);
	scene->SetGameClearInputAccepted(gameClearInputAccepted);

	// 3) ステージ解放処理はそのまま残す
	auto& repo = StageRepository::GetInstance();
	auto stages = repo.GetStages();
	auto startIndexOpt = repo.GetStartIndex();

	if (startIndexOpt && !stages.empty())
	{
		int idx = *startIndexOpt;         // 今回プレイしていたステージ
		int next = idx + 1;

		if (next < (int)stages.size())
		{
			// 次ステージを解放
			if (stages[next].locked) {
				stages[next].locked = false;
				stages[next].justUnlocked = true;
			}

			// 次回セレクト画面での初期選択を「次ステージ」にしておく
			repo.SetStartIndex(next);
		}

		repo.SetStages(stages);
	}
}

void GamePlayingState::GoToNextStage(GamePlayScene* scene)
{
	auto* input = scene->GetInput();

	input->SetLockCursor(false);
	ShowCursor(true);

	// 今までボス撃破時に直接呼んでいたステージクリア処理をここで使う
	OnStageClear(scene);
}

void GamePlayingState::CheckCollisions(GamePlayScene* scene)
{
	auto* player_ = scene->GetPlayer();
	auto& enemies_ = scene->GetEnemies();
	auto* itemManager_ = scene->GetItemManager();
	auto& levelObjectManager_ = scene->GetLevelObjectManager();
	auto* collisionManager_ = scene->GetCollisionManager();
	auto& boss_ = scene->GetBoss();

	// 衝突マネージャのリセット
	collisionManager_->Reset();

	// レベルオブジェクトのコライダーを登録
	for (auto& uptr : levelObjectManager_->GetWorldColliders())
	{
		collisionManager_->AddCollider(uptr.get());
	}

	// コライダーをリストに登録
	collisionManager_->AddCollider(player_); // プレイヤー
	player_->RegisterColliders(collisionManager_);

	// 敵キャラクターのコライダーを登録
	for (auto& e : enemies_)
	{
		if (e->IsActive()) {
			collisionManager_->AddCollider(e.get());
		}
	}

	if (boss_) {
		collisionManager_->AddCollider(boss_.get());
	}

	// アイテムのコライダーを登録
	itemManager_->RegisterColliders(collisionManager_);

	// 衝突判定と応答
	collisionManager_->CheckAllCollisions();
}
