#include "EnemySpawner.h"
#include <imgui.h>

void EnemySpawner::Initialize()
{
	currentWave_ = 1;
	StartNextWave();
}

void EnemySpawner::Update()
{
	spawnTimer_ += 1.0f / 60.0f;

	// 敵を順番に出現させる
	if (spawnedEnemies_ < totalEnemiesThisWave_ && spawnTimer_ >= spawnInterval_)
	{
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetTarget(player_); // プレイヤーをターゲットに設定
		// ランダムな出現位置（例: Z固定、Xはランダム）
		float x = static_cast<float>((rand() % 400) - 200);
		enemy->SetTranslate({ x, 0.0f, 300.0f });
		enemies_.push_back(std::move(enemy));
		spawnTimer_ = 0.0f;
		++spawnedEnemies_;
	}

	// 敵の更新
	for (auto& enemy : enemies_)
	{
		enemy->Update();
	}

	// 死亡した敵は削除（または別のロジックで管理）
	enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
		[](const std::unique_ptr<Enemy>& e) { return e->IsDead(); }), enemies_.end());
}

void EnemySpawner::Draw()
{
	for (auto& enemy : enemies_) {
		enemy->Draw();
	}
}

void EnemySpawner::DrawImGui()
{
	ImGui::Text("Wave: %d", currentWave_ - 1);
	ImGui::Text("Enemies Remaining: %zu", enemies_.size());
}

void EnemySpawner::StartNextWave()
{
	enemies_.clear();
	spawnedEnemies_ = 0;
	spawnTimer_ = 0.0f;
	totalEnemiesThisWave_ = 3 + currentWave_ * 2; // Waveごとに増える
	spawnInterval_ = 0.5f;
	++currentWave_;
}
