#include "EnemyManager.h"
#include "CollisionManager.h"
#include "ItemManager.h"

#include <cstdlib>
#include <imgui.h>


/// ------------------------------------------------------------------
///				　			初期化処理
/// ------------------------------------------------------------------
void EnemyManager::Initialize(Player* player)
{
	player_ = player; // プレイヤーへのポインタを設定
	StartNextWave();

	itemDropTable_.SetDropChance(60); // 全体ドロップ率60%
	itemDropTable_.AddEntry(ItemType::HealSmall, 30); // 小回復アイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::AmmoSmall, 40); // 小弾薬アイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::ScoreBonus, 20); // スコアボーナスアイテムのドロップ率
	itemDropTable_.AddEntry(ItemType::PowerUp, 10); // パワーアップアイテムのドロップ率
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

		// 方向をランダムに選択
		EnemySpawnDirection direction = static_cast<EnemySpawnDirection>(rand() % 4);
		Vector3 spawnPos;
		GetSpawnPosition(spawnPos, direction);
		enemy->SetTranslate(spawnPos);

		enemies_.push_back(std::move(enemy));
		spawnTimer_ = 0.0f;
		++spawnedEnemies_;
	}

	// 敵の更新
	for (auto& enemy : enemies_)
	{
		enemy->Update();

		if (enemy->IsDead() && itemManager_)
		{
			ItemType dropType;
			if (itemDropTable_.RollForDrop(dropType))
			{
				Vector3 dropPos = enemy->GetWorldTransform()->translate_;
				itemManager_->Spawn(dropType, dropPos);
			}
		}
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

void EnemyManager::GetSpawnPosition(Vector3& outPosition, EnemySpawnDirection direction)
{
	switch (direction)
	{
	case EnemySpawnDirection::Left:
		outPosition = { -200.0f, 0.0f, 0.0f }; // 左からスポーン
		break;
	case EnemySpawnDirection::Right:
		outPosition = { 200.0f, 0.0f, 0.0f }; // 右からスポーン
		break;
	case EnemySpawnDirection::Front:
		outPosition = { 0.0f, 0.0f, 200.0f }; // 前からスポーン
		break;
	case EnemySpawnDirection::Back:
		outPosition = { 0.0f, 0.0f, -200.0f }; // 後ろからスポーン
		break;
	}
}

Enemy* EnemyManager::SpawnEnemy(EnemyType type, const Vector3& position)
{
	auto enemy = std::make_unique<Enemy>();
	enemy->type_ = type; // エネミーのタイプを設定

	switch (type) {
	case EnemyType::Tank:
		enemy->SetScale({ 6.0f, 6.0f, 6.0f });
		// タンク特有のパラメータもここで設定
		break;
	case EnemyType::Sniper:
		// スナイパー専用のステータスなど
		break;
	case EnemyType::Boss:
		// ボス用設定
		break;
		// 追加しやすくなります
	}

	enemy->Initialize();
	enemy->SetTarget(player_); // プレイヤーをターゲットに設定
	enemy->SetTranslate(position);
	auto rawPtr = enemy.get();
	enemies_.push_back(std::move(enemy));
	return rawPtr;
}