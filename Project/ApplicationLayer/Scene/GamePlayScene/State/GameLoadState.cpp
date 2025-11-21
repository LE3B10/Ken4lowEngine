#include "GameLoadState.h"
#include "GamePlayScene.h"
#include "GameFadeIn.h"
#include <LevelLoader.h>
#include "StageRepository.h"

namespace
{
	using std::vector;

	// ここを好きなように調整すればOK
	const std::array<StageConfig, 5> kStageConfigs = {
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			// waves
			{
				WaveConfig{ 3, 25.0f },
				WaveConfig{ 5, 30.0f },
			},
			// bossHp
			1000.0f,
			// bossSpawnPos
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage1: 基本パラメータ
			200.0f,  // maxHp
			0.03f,   // walk
			0.08f,   // chase
			25.0f,   // damage
			0.80f,   // cooldown
			10.0f    // detect
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{ 5, 20.0f },
				WaveConfig{ 7, 25.0f },
				WaveConfig{10, 30.0f },
			},
			2500.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage2: ちょい強化
			400.0f,
			0.032f,
			0.09f,
			30.0f,
			0.75f,
			11.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{ 8, 20.0f },
				WaveConfig{12, 25.0f },
			},
			4000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage3: さらに強化
			800.0f,
			0.034f,
			0.10f,
			35.0f,
			0.70f,
			12.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{10, 20.0f },
				WaveConfig{14, 25.0f },
			},
			6000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage4
			1000.0f,
			0.036f,
			0.11f,
			40.0f,
			0.65f,
			13.0f
		},
		StageConfig{
			"Stage1.json", "Stage1.gltf",
			{
				WaveConfig{12, 20.0f },
				WaveConfig{16, 25.0f },
				WaveConfig{20, 30.0f },
			},
			8000.0f,
			Vector3{ 0.0f, 2.5f, 0.0f },
			// Stage5: 地獄
			2000.0f,
			0.038f,
			0.12f,
			50.0f,
			0.60f,
			14.0f
		},
	};
}

void GameLoadState::Enter(GamePlayScene* scene)
{
	// シーンの状態をロード中に設定
	scene->SetState(GamePlayScene::State::Loading);

	timer_ = 0.0f;

	// フェードアウト後なので、真っ黒を維持
	scene->SetFadeAlpha(1.0f);

	auto* player = scene->GetPlayer();
	auto levelLoader = std::make_unique<LevelLoader>();
	auto& levelObjectManager = scene->GetLevelObjectManager();
	auto& currentStageConfig = scene->GetCurrentWaveConfig();

	// StageSelectScene から渡されたインデックスを読む
	int stageIndex = 0;
	if (auto idxOpt = StageRepository::GetInstance().GetStartIndex()) {
		stageIndex = std::clamp(*idxOpt, 0, (int)kStageConfigs.size() - 1);
	}

	// シーンに「今のステージ番号」を保存
	scene->SetCurrentStageIndex(stageIndex);

	// ステージ設定をコピー
	currentStageConfig = kStageConfigs[stageIndex];

	// ステージ読み込みを差し替え
	levelObjectManager = std::make_unique<LevelObjectManager>();
	levelObjectManager->Initialize(
		*levelLoader->LoadLevel(currentStageConfig.levelJson),
		currentStageConfig.levelModel
	);

	player->Initialize();
	player->SetLevelObjectManager(levelObjectManager.get());
}

void GameLoadState::Update(GamePlayScene* scene, float deltaTime)
{
	// シーンが有効か確認
	if (!scene) { return; }

	auto& levelObjectManager = scene->GetLevelObjectManager();
	auto* player = scene->GetPlayer();
	auto& enemies = *scene->GetEnemies();
	auto* skybox = scene->GetSkyBox();

	// プレイヤー更新
	player->Update(deltaTime);

	// 敵更新
	for (auto& e : enemies)
	{
		e->Update(deltaTime);
	}

	// スカイボックス更新
	skybox->Update();

	// レベルオブジェクトマネージャー更新
	levelObjectManager->Update();

	// タイマー更新
	timer_ += deltaTime;

	// ロード完了＆演出時間経過で遷移先へ
	if (timer_ >= duration_)
	{
		// ロードが終わったらフェードインステートへ
		scene->ChangeState(std::make_unique<GameFadeIn>());
	}
}

void GameLoadState::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}
