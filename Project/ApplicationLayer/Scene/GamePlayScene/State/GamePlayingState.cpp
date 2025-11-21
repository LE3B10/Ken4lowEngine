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

	// Wave 情報をシーンから取得
	auto& waveConfigs = *scene->GetWaveConfigs();

	// シーン側の状態を初期化
	scene->SetCurrentWaveIndex(0);
	scene->SetAllWavesCleared(false);
	scene->SetBossSpawned(false);

	// 敵 / ボスもクリア
	auto* enemies = scene->GetEnemies();
	enemies->clear();
	scene->GetBoss().reset();

	// 最初の Wave を出す
	if (!waveConfigs.empty()) {
		SpawnWave(scene, 0);
	}

	auto* input = scene->GetInput();
	input->SetLockCursor(true);
	ShowCursor(false);
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
	std::vector<std::unique_ptr<Enemy>>& enemies = *scene->GetEnemies();
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
				const Vector3 pos = e->GetDropPosAtDeath();
				itemManager->Spawn(drop, pos);
			}
		}
	}

	// アイテム更新
	itemManager->Update(player, deltaTime);

	// レベルオブジェクト更新
	levelObjectManager->Update();

	// 通常敵の死亡したやつを消す
	enemies.erase(
		std::remove_if(
			enemies.begin(), enemies.end(),
			[](const std::unique_ptr<Enemy>& e) {
				return e->IsDeadNow();
			}),
		enemies.end()
	);

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
		if (boss && boss->IsDeadNow())
		{
			input->SetLockCursor(false);
			ShowCursor(true);
			OnStageClear(scene);
		}
	}
}

void GamePlayingState::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}

void GamePlayingState::SpawnWave(GamePlayScene* scene, int waveIndex)
{
	auto& waveConfigs = *scene->GetWaveConfigs();
	auto* player = scene->GetPlayer();
	std::vector<std::unique_ptr<Enemy>>& enemies = *scene->GetEnemies();
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
	auto& currentStageConfig = scene->GetCurrentWaveConfig();
	auto* player = scene->GetPlayer();
	auto& levelObjectManager = scene->GetLevelObjectManager();
	std::unique_ptr<Enemy>& boss = scene->GetBoss();

	if (scene->IsBossSpawned()) return;
	scene->SetBossSpawned(true);

	boss = std::make_unique<Enemy>();
	boss->Initialize();
	boss->SetPlayerPointer(player);
	boss->SetLevelObjectManager(levelObjectManager.get());

	boss->SetSpawnPosition(currentStageConfig.bossSpawnPos);
	boss->SetBoss(true, currentStageConfig.bossHp);

	// ボスは敵より少し強めにする
	boss->ApplyStageParams(
		currentStageConfig.bossHp,
		currentStageConfig.enemyChaseSpeed * 0.8f,   // 徘徊はあまり速くなくてもOK
		currentStageConfig.enemyChaseSpeed * 1.2f,   // 追跡はちょい速く
		currentStageConfig.enemyAttackDamage * 1.5f, // ダメージアップ
		currentStageConfig.enemyAttackCooldown * 0.8f,
		currentStageConfig.enemyDetectRadius + 3.0f
	);

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
