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

	// 実際の開始位置は Scene 側で決める
	player_->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

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

	e.SetParticleEffectSystem(&enemyParticleEffectSystem_);

	if (player_)
	{
		e.SetTarget(player_.get());

		e.SetOnPlayerHitUICallback([this](bool isHeadshot)
			{
				if (player_)
				{
					player_->NotifyEnemyHitUI(isHeadshot);
				}
			});

		e.SetOnPlayerKillUICallback([this](bool isHeadshot)
			{
				if (player_)
				{
					player_->NotifyEnemyKillUI(isHeadshot);
				}
			});
	}

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

	// --------------------------------------------------------
	// Initialize前に archetype を設定
	// - Initialize 内で SetArchetype(archetype_) が走っても
	//   ここで設定した種類がそのまま再適用される
	// --------------------------------------------------------
	e->SetArchetype(type);

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

void CharacterWorld::ReapplyEnemyTunings()
{
	// --------------------------------------------------------
	// Repository の最新値を、今いる全Enemyへ反映する
	// - archetype は各Enemyが持っている
	// - SetArchetype() を呼び直せば tuning を再取得して反映できる
	// --------------------------------------------------------
	for (auto& e : enemies_)
	{
		if (!e) continue;
		e->SetArchetype(e->GetArchetype());
	}
}

void CharacterWorld::DrawImGui()
{
#ifdef USE_IMGUI
	// --------------------------------------------------------
	// 1) Player の ImGui
	// --------------------------------------------------------
	if (player_)
	{
		player_->DrawImGui();
	}

	// --------------------------------------------------------
	// 2) Enemy個体ごとの ImGui
	// - ここでは各敵の Object3D / Transform などだけ出す
	// - EnemyTuningEditor は出さない
	// --------------------------------------------------------
	for (auto& e : enemies_)
	{
		e->DrawImGui();
	}

	// --------------------------------------------------------
	// 3) EnemyTuningEditor は 1回だけ描画する
	// - 保存 / 削除 / Reload 後に全Enemyへ再反映する
	// --------------------------------------------------------
	EnemyTuningEditorHooks hooks{};

	hooks.onSaved = [this](EnemyArchetype /*type*/)
		{
			ReapplyEnemyTunings();
		};

	hooks.onDeleted = [this](EnemyArchetype /*type*/)
		{
			ReapplyEnemyTunings();
		};

	hooks.onReloaded = [this]()
		{
			ReapplyEnemyTunings();
		};

	tuningEditor_.Draw(hooks);
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