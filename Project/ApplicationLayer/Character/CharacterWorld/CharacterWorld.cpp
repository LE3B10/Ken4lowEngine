#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "EnemyFactory.h"
#include "MeleeEnemy.h"
#include "MidRangeEnemy.h"
#include <Stage.h>
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
		case EnemyType::MidRange: return 1;
		case EnemyType::Melee:
		default: return 0;
		}
	}
}

void CharacterWorld::Initialize(GameContext& ctx)
{
	ctx_ = ctx;
	playerRuntimeOverride_ = nullptr;
	legacyPlayerProxyMode_ = false;
	playerMigrationRuntime_ = std::make_unique<GamePlayPlayerMigrationRuntime>();
	enemyParticleEffectSystem_.Initialize();

	player_ = std::make_unique<Player>();
	InjectPlayerDeps(*player_);
	player_->Initialize();
	// 旧BaseCharacterが持っていた初期Y=2.25を復元し、PlayerSpawnPointが無いStageでも地面へ半分埋まらないようにする。
	player_->SetSpawnPosition({ 0.0f, 2.25f, 0.0f });
	player_->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(player_->GetCollisionPrimitive()); // Playerは共通CharacterColliderComponentだけを登録する。
	}

	enemies_.clear();
	notifiedKilledEnemies_.clear();
	spawnedEnemyCounts_.fill(0);
}

void CharacterWorld::Finalize()
{
	ClearEnemies();

	if (playerMigrationRuntime_)
	{
		playerMigrationRuntime_->Finalize();
		playerMigrationRuntime_.reset(); // Stageや旧Playerを破棄する前に新Player側の非所有参照を解除する。
	}
	playerRuntimeOverride_ = nullptr;
	legacyPlayerProxyMode_ = false;

	if (ctx_.collisionManager_ && player_)
	{
		ctx_.collisionManager_->RemoveCollider(player_->GetCollisionPrimitive());
	}
	player_.reset();
	ctx_ = GameContext{};
}

void CharacterWorld::EnsurePlayerMigrationRuntime()
{
	if (!enablePlayerMigrationRuntime_ || legacyPlayerProxyMode_ || !player_ || !ctx_.bulletManager_) return;
	K4E::Stage* stage = K4E::Stage::GetActiveRuntimeStage();
	if (!stage) return;
	if (!playerMigrationRuntime_) playerMigrationRuntime_ = std::make_unique<GamePlayPlayerMigrationRuntime>();
	if (!playerMigrationRuntime_->Initialize(player_.get(), ctx_.bulletManager_, stage)) return;

	SetPlayerRuntimeOverride(playerMigrationRuntime_->GetPlayerRuntime());
	SetLegacyPlayerProxyMode(true); // 新Playerを正本にした後もEnemy/Boss等の旧Player参照は位置同期Proxyとして維持する。
}

void CharacterWorld::UpdateActivePlayer(float deltaTime)
{
	EnsurePlayerMigrationRuntime();
	if (playerMigrationRuntime_ && playerMigrationRuntime_->IsActive())
	{
		playerMigrationRuntime_->SyncAfterLegacyCollision();
		playerMigrationRuntime_->Update(deltaTime);
		return;
	}
	if (player_) player_->Update(deltaTime);
}

void CharacterWorld::InjectPlayerDeps(Player& player)
{
	player.SetCollisionManager(ctx_.collisionManager_);
	player.SetBulletManager(ctx_.bulletManager_);
	player.SetOnHitSECallback([]() { AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.2f); });
	player.SetOnFireSECallback([]() { AudioManager::GetInstance()->PlaySE("player_fire.mp3", 0.1f); });
	player.SetOnReloadSECallback([]() { AudioManager::GetInstance()->PlaySE("enemy_reload.mp3", 0.2f); });
	player.SetOnDeathSECallback([]() { AudioManager::GetInstance()->PlaySE("enemy_death.mp3", 0.2f); });
}

void CharacterWorld::InjectEnemyDeps(EnemyBase& enemy)
{
	if (dynamic_cast<MidRangeEnemy*>(&enemy) == nullptr) enemy.SetParticleEffectSystem(&enemyParticleEffectSystem_);

	if (auto* meleeEnemy = dynamic_cast<MeleeEnemy*>(&enemy))
	{
		if (player_) meleeEnemy->SetTarget(player_.get());
	}
	else if (auto* midRangeEnemy = dynamic_cast<MidRangeEnemy*>(&enemy))
	{
		if (player_) midRangeEnemy->SetTarget(player_.get());
	}
}

std::vector<EnemyBase*> CharacterWorld::GetEnemyRawList() const
{
	std::vector<EnemyBase*> result;
	result.reserve(enemies_.size());
	for (const auto& enemy : enemies_) result.push_back(enemy.get());
	return result;
}

EnemyBase& CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	auto enemy = EnemyFactory::Create(request.enemyType);
	InjectEnemyDeps(*enemy);
	enemy->Initialize();
	enemy->SetPosition(request.position);

	if (ctx_.collisionManager_)
	{
		ctx_.collisionManager_->AddCollider(enemy->GetCollisionPrimitive()); // Enemy自身ではなく共通Component所有Colliderだけを登録する。
	}

	enemies_.push_back(std::move(enemy));
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
		if (!enemy || enemy->IsDead()) continue;
		if (dynamic_cast<const MeleeEnemy*>(enemy.get()) || dynamic_cast<const MidRangeEnemy*>(enemy.get())) ++aliveCount;
	}
	return aliveCount;
}

void CharacterWorld::ClearEnemies()
{
	notifiedKilledEnemies_.clear();
	if (ctx_.collisionManager_)
	{
		for (auto& enemy : enemies_)
		{
			if (enemy) ctx_.collisionManager_->RemoveCollider(enemy->GetCollisionPrimitive());
		}
	}
	enemies_.clear();
}

bool CharacterWorld::RemoveEnemy(EnemyBase* enemy)
{
	if (!enemy) return false;
	const auto it = std::find_if(enemies_.begin(), enemies_.end(), [enemy](const std::unique_ptr<EnemyBase>& entry) { return entry.get() == enemy; });
	if (it == enemies_.end()) return false;

	notifiedKilledEnemies_.erase(enemy);
	if (ctx_.collisionManager_) ctx_.collisionManager_->RemoveCollider(enemy->GetCollisionPrimitive());
	enemies_.erase(it);
	return true;
}

void CharacterWorld::Update(float deltaTime)
{
	UpdateActivePlayer(deltaTime);

	const float enemyDeltaTime = std::clamp(deltaTime, 0.0f, EnemyBase::GetMaxUpdateDeltaTime());
	for (auto& enemy : enemies_)
	{
		const bool wasAlreadyNotified = notifiedKilledEnemies_.contains(enemy.get());
		enemy->Update(enemyDeltaTime);
		if (!wasAlreadyNotified && enemy->IsDead())
		{
			notifiedKilledEnemies_.insert(enemy.get());
			if (onEnemyKilled_) onEnemyKilled_(enemy->GetCenterPosition());
		}
	}

	if (ctx_.collisionManager_)
	{
		enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(), [&](const std::unique_ptr<EnemyBase>& enemy)
			{
				if (!enemy || !enemy->IsRemovable()) return false;
				notifiedKilledEnemies_.erase(enemy.get());
				ctx_.collisionManager_->RemoveCollider(enemy->GetCollisionPrimitive());
				return true;
			}), enemies_.end());
	}
}

void CharacterWorld::UpdatePlayerOnly(float deltaTime)
{
	UpdateActivePlayer(deltaTime);
}

void CharacterWorld::WarmupStartGameplayVisuals()
{
	EnsurePlayerMigrationRuntime();
	if (playerMigrationRuntime_ && playerMigrationRuntime_->IsActive()) playerMigrationRuntime_->Update(0.0f, false);
	else if (player_) player_->WarmupStartGameplayVisuals();
	for (auto& enemy : enemies_) if (enemy) enemy->Update(0.0f);
}

void CharacterWorld::SetStartGameplayVisualsVisible(bool visible)
{
	if (!legacyPlayerProxyMode_ && player_) player_->SetStartGameplayVisualsVisible(visible);
}

void CharacterWorld::Draw()
{
	if (playerMigrationRuntime_ && playerMigrationRuntime_->IsActive()) playerMigrationRuntime_->Draw();
	else if (player_) player_->Draw();
	for (auto& enemy : enemies_) if (enemy) enemy->Draw();
}

void CharacterWorld::DrawImGui()
{
#ifdef USE_IMGUI
	DrawPlayerDebugImGui();
	DrawEnemyDebugImGui();
#endif
}

void CharacterWorld::DrawPlayerDebugImGui()
{
#ifdef USE_IMGUI
	if (!player_)
	{
		ImGui::TextUnformatted("Player: N/A");
		return;
	}

	ImGui::SeparatorText("P10 Player Migration");
	ImGui::Text("New Player Runtime: %s", playerMigrationRuntime_ && playerMigrationRuntime_->IsActive() ? "ACTIVE" : "WAITING");
	ImGui::Text("Legacy Proxy Mode: %s", legacyPlayerProxyMode_ ? "ON" : "OFF");
	if (const K4E::PlayerActor* migrated = GetMigratedPlayerActor())
	{
		const K4E::Vector3 position = migrated->GetRootComponent() ? migrated->GetRootComponent()->GetWorldPosition() : K4E::Vector3{};
		ImGui::Text("New Player Pos: %.2f, %.2f, %.2f", position.x, position.y, position.z);
		ImGui::Text("New Player HP: %.1f / %.1f", migrated->GetHP(), migrated->GetMaxHP());
		for (const auto& componentOwner : migrated->GetComponents())
		{
			if (!componentOwner) continue;
			K4E::ActorComponent* component = componentOwner.get();
			const std::string label = component->GetName().empty() ? component->GetClassTypeName() : component->GetName();
			ImGui::PushID(component);
			if (ImGui::CollapsingHeader(label.c_str())) component->DrawImGui();
			ImGui::PopID();
		}
	}

	ImGui::SeparatorText("Legacy Player Proxy");
	player_->DrawPlayerDebugImGui();
#endif
}

void CharacterWorld::DrawEnemyDebugImGui()
{
#ifdef USE_IMGUI
	int totalStuckDetections = 0;
	int totalStuckRecoveries = 0;
	for (const auto& enemy : enemies_)
	{
		if (!enemy) continue;
		totalStuckDetections += enemy->GetStuckDetectionCount();
		totalStuckRecoveries += enemy->GetStuckRecoveryCount();
	}
	ImGui::Text("スタック検出回数: %d", totalStuckDetections);
	ImGui::Text("スタック復帰回数: %d", totalStuckRecoveries);
	if (!enemies_.empty() && enemies_.front())
	{
		const Vector3 position = enemies_.front()->GetCenterPosition();
		ImGui::Text("敵の現在座標: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
	}

	std::array<int, 2> liveEnemyCounts{};
	for (const auto& enemy : enemies_)
	{
		if (dynamic_cast<const MeleeEnemy*>(enemy.get())) ++liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Melee)];
		else if (dynamic_cast<const MidRangeEnemy*>(enemy.get())) ++liveEnemyCounts[ToEnemyTypeIndex(EnemyType::MidRange)];
	}

	ImGui::Text("現在の敵数: %d", static_cast<int>(enemies_.size()));
	ImGui::Text("現在の近接雑魚敵数: %d", liveEnemyCounts[ToEnemyTypeIndex(EnemyType::Melee)]);
	ImGui::Text("現在の中距離雑魚敵数: %d", liveEnemyCounts[ToEnemyTypeIndex(EnemyType::MidRange)]);
	ImGui::Separator();
	ImGui::Text("生成済み敵数: %d", spawnedEnemyCounts_[0] + spawnedEnemyCounts_[1]);
	ImGui::Text("近接雑魚敵数: %d", spawnedEnemyCounts_[ToEnemyTypeIndex(EnemyType::Melee)]);
	ImGui::Text("中距離雑魚敵数: %d", spawnedEnemyCounts_[ToEnemyTypeIndex(EnemyType::MidRange)]);

	constexpr const char* kEnemyTypeLabels[] = { "近接雑魚敵", "中距離雑魚敵" };
	int debugSpawnEnemyTypeIndex = static_cast<int>(debugSpawnEnemyType_);
	if (ImGui::Combo("敵種別", &debugSpawnEnemyTypeIndex, kEnemyTypeLabels, IM_ARRAYSIZE(kEnemyTypeLabels)))
	{
		debugSpawnEnemyType_ = static_cast<EnemyType>(debugSpawnEnemyTypeIndex);
	}

	if (player_ && ImGui::Button("選択した敵を生成"))
	{
		const Vector3 spawnPosition = player_->GetCenterPosition() + Vector3{ 0.0f, 0.0f, 3.0f };
		SpawnEnemyAt(spawnPosition, debugSpawnEnemyType_);
	}
	ImGui::Separator();
#endif
}
