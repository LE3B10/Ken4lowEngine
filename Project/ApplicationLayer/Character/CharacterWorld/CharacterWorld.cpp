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
	return *enemies_.back();
}

EnemyBase& CharacterWorld::SpawnEnemyAt(const K4E::Vector3& position, EnemyType enemyType)
{
	EnemySpawnRequest request{};
	request.position = position;
	request.enemyType = enemyType;
	return SpawnEnemy(request);
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

	for (auto& e : enemies_)
	{
		const bool wasAlreadyNotified = notifiedKilledEnemies_.contains(e.get());
		e->Update(dt);

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
	// Enemy DebugにはEnemy Manager相当の一覧と各Enemy個体の詳細をまとめる。
	ImGui::Text("Enemy Count: %d", static_cast<int>(enemies_.size()));
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
