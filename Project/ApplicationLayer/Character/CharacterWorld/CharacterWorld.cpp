#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include <algorithm>

#include "WorldCollisionResolver.h"
#include "AudioManager.h"
#include <imgui.h>

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
	spawnSequences_.clear();
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

Enemy& CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	auto e = std::make_unique<Enemy>();
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

Enemy& CharacterWorld::SpawnEnemyAt(const K4E::Vector3& position, int spawnPointIndex, int waveNumber)
{
	EnemySpawnRequest request{};
	request.position = position;
	request.spawnPointIndex = spawnPointIndex;
	request.waveNumber = waveNumber;
	Enemy& enemy = SpawnEnemy(request);
	enemy.SetSpawnPresentationActive(true, 0.0f);
	EnemySpawnSequenceState state{};
	state.request = request;
	state.enemy = &enemy;
	spawnSequences_.push_back(state);
	return enemy;
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
	auto* pm = K4E::GpuParticleManager::GetInstance();
	for (auto& seq : spawnSequences_)
	{
		seq.timer += dt;
		if (!seq.telegraphPlayed && seq.timer >= 0.0f)
		{
			if (auto* e = pm->GetEmitter("Spawn_Telegraph_Ground")) { e->SetPosition(seq.request.position); e->RequestEmit(1); }
			seq.telegraphPlayed = true;
		}
		if (!seq.convergePlayed && seq.timer >= 0.1f)
		{
			if (auto* e = pm->GetEmitter("Spawn_Converge")) { e->SetPosition(seq.request.position); e->RequestEmit(1); }
			seq.convergePlayed = true;
		}
		if (!seq.materializePlayed && seq.timer >= 0.35f)
		{
			if (auto* e = pm->GetEmitter("Spawn_Materialize")) { e->SetPosition(seq.request.position); e->RequestEmit(1); }
			seq.materializePlayed = true;
		}
		if (seq.enemy)
		{
			const float fadeT = (seq.timer - 0.35f) / 0.45f;
			seq.enemy->SetSpawnPresentationActive(seq.timer < 0.8f, fadeT);
		}
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

void CharacterWorld::DrawImGui()
{
#ifdef USE_IMGUI
	// --------------------------------------------------------
	// 1) Player の ImGui
	// --------------------------------------------------------
	if (player_) { player_->DrawImGui(); }

	// --------------------------------------------------------
	// 2) Enemy個体ごとの ImGui
	// - ここでは各敵の Object3D / Transform などだけ出す
	// - EnemyTuningEditor は出さない
	// --------------------------------------------------------
	for (auto& e : enemies_) { e->DrawImGui(); }
	if (ImGui::Begin("Enemy Spawn Sequence"))
	{
		for (size_t i = 0; i < spawnSequences_.size(); ++i)
		{
			const auto& s = spawnSequences_[i];
			ImGui::Text("Seq[%d] wave=%d spawnPoint=%d pos=(%.2f, %.2f, %.2f) timer=%.2f", static_cast<int>(i), s.request.waveNumber, s.request.spawnPointIndex, s.request.position.x, s.request.position.y, s.request.position.z, s.timer);
			ImGui::Text("  telegraph=%d converge=%d materialize=%d", s.telegraphPlayed ? 1 : 0, s.convergePlayed ? 1 : 0, s.materializePlayed ? 1 : 0);
		}
	}
	ImGui::End();

	// --------------------------------------------------------
	// 3) EnemyTuningEditor は 1回だけ描画する
	// - 保存 / 削除 / Reload 後に全Enemyへ再反映する
	// --------------------------------------------------------
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
