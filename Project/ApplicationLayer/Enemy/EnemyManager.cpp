#include "EnemyManager.h"
#include "CollisionManager.h"
#include <cstdlib>
#include <imgui.h>


/// ------------------------------------------------------------------
///				　			初期化処理
/// ------------------------------------------------------------------
void EnemyManager::Initialize(Player* player)
{
	player_ = player; // プレイヤーへのポインタを設定
	StartNextWave();
}


/// ------------------------------------------------------------------
///				　			　 更新処理
/// ------------------------------------------------------------------
void EnemyManager::Update()
{
	spawnTimer_ += 1.0f / 60.0f;

	// 敵を順番に出現させる
	if (spawnedEnemies_ < totalEnemiesThisWave_ && spawnTimer_ >= spawnInterval_)
	{
		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetTarget(player_); // プレイヤーをターゲットに設定
		// ランダムな出現位置（例: Z固定、Xはランダム）
		float x = static_cast<float>((rand() % 300) - 150);
		enemy->SetTranslate({ x, 0.0f, 200.0f });
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


/// ------------------------------------------------------------------
///				　			　 描画処理
/// ------------------------------------------------------------------
void EnemyManager::Draw()
{
	for (auto& enemy : enemies_) {
		enemy->Draw();
	}
}


/// ------------------------------------------------------------------
///				　			　 コライダー登録
/// ------------------------------------------------------------------
void EnemyManager::RegisterColliders(CollisionManager* collisionManager)
{
	for (auto& enemy : enemies_)
	{
		if (!enemy->IsDead())
		{
			collisionManager->AddCollider(enemy.get());
			for (auto& bullet : enemy->GetBullets())
			{
				if (!bullet->IsDead())
				{
					collisionManager->AddCollider(bullet.get());
				}
			}
		}
	}
}


/// ------------------------------------------------------------------
///				　			　 ImGui描画処理
/// ------------------------------------------------------------------
void EnemyManager::DrawImGui()
{
	ImGui::Text("Wave: %d", currentWave_ - 1);
	ImGui::Text("Enemies Remaining: %zu", enemies_.size());
}


/// ------------------------------------------------------------------
///				　			　 次のウェーブを開始
/// ------------------------------------------------------------------
void EnemyManager::StartNextWave()
{
	enemies_.clear();
	spawnedEnemies_ = 0;
	spawnTimer_ = 0.0f;
	totalEnemiesThisWave_ = 3 + currentWave_ * 2; // Waveごとに増える
	spawnInterval_ = 0.5f;
	++currentWave_;
}
