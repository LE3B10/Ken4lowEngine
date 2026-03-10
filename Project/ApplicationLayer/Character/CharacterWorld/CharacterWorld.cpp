#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include <algorithm>

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

	// ★ デバッグ用の初期スポーン/オフセットは入れない
	//   実際の開始位置は GamePlayScene 側の PlayerSpawnPoint で決める
	player_->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

	// Collider登録（PlayerはColliderとして扱われている前提）
	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(player_.get());
	}

	enemies_.clear();
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

void CharacterWorld::InjectEnemyDeps(Enemy& e)
{
	e.SetCollisionManager(ctx_.collisionManager_);
	e.SetBulletManager(ctx_.bulletManager_);

	e.SetParticleEffectSystem(&enemyParticleEffectSystem_); // 敵の被弾エフェクトシステムを渡す

	// Enemyのターゲットは Player(Collider) を渡す（DebugSceneと同じ）
	if (player_)
	{
		e.SetTarget(player_.get());

		// 命中UI（ヒットマーカー）
		e.SetOnPlayerHitUICallback([this](bool isHeadshot)
			{
				if (player_)
				{
					player_->NotifyEnemyHitUI(isHeadshot);
				}
			});

		// 撃破UI（キル確認マーカー）
		e.SetOnPlayerKillUICallback([this](bool isHeadshot)
			{
				if (player_)
				{
					player_->NotifyEnemyKillUI(isHeadshot);
				}
			});
	}

	// -----------------------------
	// 敵SE
	// -----------------------------
	e.SetOnHitSECallback([]()
		{
			AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.2f);
		});

	e.SetOnFireSECallback([]()
		{
			AudioManager::GetInstance()->PlaySE("enemy_fire.mp3", 0.2f);
		});

	e.SetOnReloadSECallback([]()
		{
			AudioManager::GetInstance()->PlaySE("enemy_reload.mp3", 0.2f);
		});

	e.SetOnDeathSECallback([]()
		{
			AudioManager::GetInstance()->PlaySE("enemy_death.mp3", 0.2f);
		});
}

Enemy& CharacterWorld::SpawnEnemy(const K4E::Vector3& pos)
{
	auto e = std::make_unique<Enemy>();
	InjectEnemyDeps(*e);

	e->Initialize(pos);

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	return *enemies_.back();
}

Enemy& CharacterWorld::SpawnEnemy(EnemyArchetype type, const K4E::Vector3& pos)
{
	auto e = std::make_unique<Enemy>();
	e->SetArchetype(type); // Initialize前に反映（視覚/射撃距離など）
	InjectEnemyDeps(*e);
	e->Initialize(pos);

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	return *enemies_.back();
}


void CharacterWorld::ClearEnemies()
{
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
		e->Update(dt);
	}

	// 死亡・削除対象の掃除（IsRemovableで判定）
	if (ctx_.collisionManager_)
	{
		enemies_.erase(
			std::remove_if(enemies_.begin(), enemies_.end(),
				[&](const std::unique_ptr<Enemy>& e)
				{
					if (e && e->IsRemovable())
					{
						ctx_.collisionManager_->RemoveCollider(e.get());
						return true;
					}
					return false;
				}),
			enemies_.end());
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
	if (player_) player_->DrawImGui();
	for (auto& e : enemies_) e->DrawImGui();
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
