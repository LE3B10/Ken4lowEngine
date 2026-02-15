#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include <algorithm>

void CharacterWorld::Initialize(GameContext& ctx)
{
	ctx_ = ctx;

	// --- Player ---
	player_ = std::make_unique<Player>();
	InjectPlayerDeps(*player_);
	player_->Initialize();

	// Collider登録（PlayerはColliderとして扱われている前提：DebugSceneと同じ）
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
}

void CharacterWorld::InjectEnemyDeps(Enemy& e)
{
	e.SetCollisionManager(ctx_.collisionManager_);
	e.SetBulletManager(ctx_.bulletManager_);

	// Enemyのターゲットは Player(Collider) を渡す（DebugSceneと同じ）
	if (player_)
	{
		e.SetTarget(player_.get());
	}
}

Enemy& CharacterWorld::SpawnEnemy(const K4E::Vector3& pos, const std::string& modelPath)
{
	auto e = std::make_unique<Enemy>();
	InjectEnemyDeps(*e);

	e->Initialize(pos, modelPath);

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(e.get());
	}

	enemies_.push_back(std::move(e));
	return *enemies_.back();
}

Enemy& CharacterWorld::SpawnEnemy(EnemyArchetype type, const K4E::Vector3& pos, const std::string& modelPath)
{
	auto e = std::make_unique<Enemy>();
	e->SetArchetype(type); // Initialize前に反映（視覚/射撃距離など）
	InjectEnemyDeps(*e);
	e->Initialize(pos, modelPath);

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
	if (isDebug_) return; // デバッグモードなら更新しない（描画だけする）

	if (player_) player_->Update(dt);

	for (auto& e : enemies_)
	{
		// Enemyは Update(dt) がある（Enemy.hの互換Updateもある）
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