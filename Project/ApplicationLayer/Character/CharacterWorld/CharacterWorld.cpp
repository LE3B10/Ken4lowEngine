#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "EnemyFactory.h"
#include "Enemy.h"
#include "MeleeEnemy.h"
#include "MidRangeEnemy.h"
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include "WorldCollisionResolver.h"
#include "AudioManager.h"

using namespace Ken4lowEngine;

namespace
{
	size_t ToEnemyTypeIndex(EnemyType enemyType)
	{
		switch (enemyType)
		{
		case EnemyType::Melee: return 1;
		case EnemyType::MidRange: return 2;
		case EnemyType::Legacy:
		default: return 0;
		}
	}
}

void CharacterWorld::Initialize(GameContext& ctx)
{
	ctx_ = ctx;

	// 敵の被弾エフェクトシステムを初期化
	enemyParticleEffectSystem_.Initialize();

	// --- Player ---
	player_ = std::make_unique<Player>();
	InjectPlayerDeps(*player_);
	player_->Initialize();

	// 実際の開始位置は Scene 側で決める
	player_->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(player_.get());
	}

	enemies_.clear();
	notifiedKilledEnemies_.clear();
	spawnedEnemyCounts_.fill(0);
}

void CharacterWorld::Finalize()
{
	ClearEnemies();

	if (ctx_.collisionManager_ && player_)
	{
		ctx_.collisionManager_->RemoveCollider(player_.get());
	}
	player_.reset();
	ctx_ = GameContext{};
}

void CharacterWorld::InjectPlayerDeps(Player& p)
{
	p.SetCollisionManager(ctx_.collisionManager_);
	p.SetBulletManager(ctx_.bulletManager_);

	p.SetOnHitSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.2f);
		});

	p.SetOnFireSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("player_fire.mp3", 0.1f);
		});

	p.SetOnReloadSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_reload.mp3", 0.2f);
		});

	p.SetOnDeathSECallback([]()
		{
			Ken4lowEngine::AudioManager::GetInstance()->PlaySE("enemy_death.mp3", 0.2f);
		});
}

void CharacterWorld::InjectEnemyDeps(EnemyBase& e)
{
	// MidRangeEnemy固有パーティクルは復活させず、既存方針を維持する。
	if (dynamic_cast<MidRangeEnemy*>(&e) == nullptr)
	{
		e.SetParticleEffectSystem(&enemyParticleEffectSystem_);
	}

	if (auto* legacyEnemy = dynamic_cast<Enemy*>(&e))
	{
		legacyEnemy->SetCollisionManager(ctx_.collisionManager_);
		legacyEnemy->SetBulletManager(ctx_.bulletManager_);
		if (player_) { legacyEnemy->SetTarget(player_.get()); }
	}
	else if (auto* meleeEnemy = dynamic_cast<MeleeEnemy*>(&e))
	{
		if (player_) { meleeEnemy->SetTarget(player_.get()); }
	}
	else if (auto* midRangeEnemy = dynamic_cast<MidRangeEnemy*>(&e))
	{
		if (player_) { midRangeEnemy->SetTarget(player_.get()); }
	}
}

std::vector<EnemyBase*> CharacterWorld::GetEnemyRawList() const
{
	std::vector<EnemyBase*> result;
	result.reserve(enemies_.size());

	for (const auto& enemy : enemies_)
	{
		result.push_back(enemy.get());
	}

	return result;
}

EnemyBase& CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	// 通常ゲームでも近接・中距離雑魚敵を生成できるよう、EnemyFactory経由で敵を作成する。
	auto e = EnemyFactory::Create(request.enemyType);
	InjectEnemyDeps(*e);
	e->Initialize();
	e->SetPosition(request.position);

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	++spawnedEnemyCounts_[ToEnemyTypeIndex(request.enemyType)];
	return *enemies_.back();
}

EnemyBase& CharacterWorld::SpawnEnemyAt(const K4E::Vector3& position, EnemyType enemyType)
{
	EnemySpawnRequest request{};
	request.position = position;
	request.enemyType = enemyType;
	return SpawnEnemy(request);
}

int CharacterWorld::GetAliveNormalEnemyCount() const
{
	int aliveCount = 0;
	for (const auto& enemy : enemies_)
	{
		if (!enemy || enemy->IsDead())
		{
			continue;
		}

		// ボスやクリスタルはCharacterWorldの雑魚敵配列に入れず、近接/中距離雑魚敵だけを進行条件に使う。
		if (dynamic_cast<const MeleeEnemy*>(enemy.get()) || dynamic_cast<const MidRangeEnemy*>(enemy.get()))
		{
			++aliveCount;
		}
	}
	return aliveCount;
}

void CharacterWorld::ClearEnemies()
{
	notifiedKilledEnemies_.clear();

	if (ctx_.collisionManager_)
	{
		for (auto& e : enemies_)
		{
			ctx_.collisionManager_->RemoveCollider(e.get());
		}
	}
	enemies_.clear();
}

void CharacterWorld::Update(float dt)
{
	if (player_) player_->Update(dt);

	const float enemyDeltaTime = std::clamp(dt, 0.0f, EnemyBase::GetMaxUpdateDeltaTime());
	for (auto& e : enemies_)
	{
		const bool wasAlreadyNotified = notifiedKilledEnemies_.contains(e.get());
		e->Update(enemyDeltaTime);

		// 衝突更新で死亡した敵も次フレームに1回だけ通知して、ドロップ生成の取り逃しを防ぐ。
		if (!wasAlreadyNotified && e->IsDead())
		{
			notifiedKilledEnemies_.insert(e.get());
			if (onEnemyKilled_)
			{
				onEnemyKilled_(e->GetCenterPosition());
			}
		}
	}

	if (ctx_.collisionManager_)
	{
		enemies_.erase(
			std::remove_if(enemies_.begin(), enemies_.end(),
				[&](const std::unique_ptr<EnemyBase>& e)
				{
					if (e && e->IsRemovable())
					{
						notifiedKilledEnemies_.erase(e.get());
						ctx_.collisionManager_->RemoveCollider(e.get());
						return true;
					}
					return false;
				}),
			enemies_.end());
	}
}

void CharacterWorld::UpdatePlayerOnly(float dt)
{
	if (player_)
	{
		player_->Update(dt);
	}
}

void CharacterWorld::WarmupStartGameplayVisuals()
{
	if (player_)
	{
		player_->WarmupStartGameplayVisuals();
	}

	for (auto& e : enemies_)
	{
		if (e)
		{
			e->Update(0.0f);
		}
	}
}

void CharacterWorld::SetStartGameplayVisualsVisible(bool visible)
{
	if (player_)
	{
		player_->SetStartGameplayVisualsVisible(visible);
	}
}

void CharacterWorld::Draw()
{
	if (player_) player_->Draw();
	for (auto& e : enemies_) e->Draw();
}

void CharacterWorld::DrawImGui()
{
#ifdef USE_IMGUI
	// 互換用の一括描画は用途別Debugパネルの中身を再利用する。
	DrawPlayerDebugImGui();
	DrawEnemyDebugImGui();
#endif
}

void CharacterWorld::DrawPlayerDebugImGui()
{
#ifdef USE_IMGUI
	// Player Debugだけを開いた時にEnemy側の重い項目を描かないよう分離する。
	if (player_) { player_->DrawPlayerDebugImGui(); }
#endif
}

void CharacterWorld::DrawEnemyDebugImGui()
{
#ifdef USE_IMGUI
	int totalStuckDetections = 0;
	int totalStuckRecoveries = 0;
	for (const auto& enemy : enemies_)
	{
		if (!enemy) { continue; }
		totalStuckDetections += enemy->GetStuckDetectionCount();
		totalStuckRecoveries += enemy->GetStuckRecoveryCount();
	}
	ImGui::Text("スタック検出回数: %d", totalStuckDetections);
	ImGui::Text("スタック復帰回数: %d", totalStuckRecoveries);
	if (!enemies_.empty() && enemies_.front())
	{
		const Vector3 pos = enemies_.front()->GetCenterPosition();
		ImGui::Text("敵の現在座標: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	}
	// 旧Enemyを段階的に置き換えるため、通常ゲーム上の敵種別ごとの生成数を確認する。
	std::array<int, 3> liveEnemyCounts{};
	for (const auto& enemy : enemies_)
	{
		if (dynamic_cast<const MeleeEnemy*>(enemy.get()))
		{
			++liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Melee)];
		}
		else if (dynamic_cast<const MidRangeEnemy*>(enemy.get()))
		{
			++liveEnemyCounts[ToEnemyTypeIndex(EnemyType::MidRange)];
		}
		else
		{
			++liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Legacy)];
		}
	}

	ImGui::Text("現在の敵数: %d", static_cast<int>(enemies_.size()));
	ImGui::Text("現在の旧Enemy数: %d", liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Legacy)]);
	ImGui::Text("現在の近接雑魚敵数: %d", liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Melee)]);
	ImGui::Text("現在の中距離雑魚敵数: %d", liveEnemyCounts[ToEnemyTypeIndex(EnemyType::MidRange)]);
	ImGui::Separator();
	ImGui::Text("生成済み敵数: %d", spawnedEnemyCounts_[0] + spawnedEnemyCounts_[1] + spawnedEnemyCounts_[2]);
	ImGui::Text("旧Enemy数: %d", spawnedEnemyCounts_[ToEnemyTypeIndex(EnemyType::Legacy)]);
	ImGui::Text("近接雑魚敵数: %d", spawnedEnemyCounts_[ToEnemyTypeIndex(EnemyType::Melee)]);
	ImGui::Text("中距離雑魚敵数: %d", spawnedEnemyCounts_[ToEnemyTypeIndex(EnemyType::MidRange)]);

	// 通常ゲーム側でもFactory接続を確認できるよう、生成する雑魚敵派生を一時的に切り替える。
	constexpr const char* kEnemyTypeLabels[] = { "旧Enemy", "近接雑魚敵", "中距離雑魚敵" };
	int debugSpawnEnemyTypeIndex = static_cast<int>(debugSpawnEnemyType_);
	if (ImGui::Combo("敵種別", &debugSpawnEnemyTypeIndex, kEnemyTypeLabels, IM_ARRAYSIZE(kEnemyTypeLabels)))
	{
		debugSpawnEnemyType_ = static_cast<EnemyType>(debugSpawnEnemyTypeIndex);
	}

	if (player_ && ImGui::Button("選択した敵を生成"))
	{
		const K4E::Vector3 spawnPosition = player_->GetCenterPosition() + K4E::Vector3{ 0.0f, 0.0f, 3.0f };
		SpawnEnemyAt(spawnPosition, debugSpawnEnemyType_);
	}

	ImGui::Separator();
	for (size_t i = 0; i < enemies_.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::TreeNode("Enemy", "Enemy %zu", i))
		{
			if (enemies_[i]) { enemies_[i]->DrawImGui(); }
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
#endif
}

void CharacterWorld::DrawShadow()
{
	if (player_) { player_->DrawShadow(); }
	for (auto& e : enemies_)
	{
		e->DrawShadow();
	}
}

void CharacterWorld::UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
{
	if (player_) { player_->UpdateShadowMatrix(lightViewProjection); }

	for (auto& e : enemies_)
	{
		e->UpdateShadowMatrix(lightViewProjection);
	}
}
