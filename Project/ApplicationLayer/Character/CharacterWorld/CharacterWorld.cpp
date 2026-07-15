#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "EnemyFactory.h"
#include "MeleeEnemy.h"
#include "MidRangeEnemy.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"
#include <Stage.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

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
	playerRuntimeController_ = std::make_unique<GamePlayPlayerMigrationRuntime>();
	enemyParticleEffectSystem_.Initialize();

	actorWorld_.SetPhysicsWorld(&physicsWorld_);
	physicsWorld_.SetUseFixedStep(false);
	actorWorld_.Initialize(); // P13以降のPlayerActorはCharacterWorld所有ActorWorldだけが所有する。

	playerSpawnPosition_ = { 0.0f, 2.25f, 0.0f };
	playerCollisionBridgeRegistered_ = false;
	enemies_.clear();
	notifiedKilledEnemies_.clear();
	spawnedEnemyCounts_.fill(0);
}

void CharacterWorld::Finalize()
{
	ClearEnemies();
	MidRangeBombProjectile::SetTargetPlayerRuntime(nullptr);
	UnregisterPlayerCollisionBridge();

	if (playerRuntimeController_)
	{
		playerRuntimeController_->Finalize();
		playerRuntimeController_.reset();
	}

	actorWorld_.Finalize();
	ctx_ = GameContext{};
}

void CharacterWorld::SetPlayerSpawnPosition(const K4E::Vector3& position)
{
	playerSpawnPosition_ = position;
	if (K4E::PlayerActor* player = GetPlayer()) player->ResetForValidation(playerSpawnPosition_);
}

void CharacterWorld::EnsurePlayerRuntime()
{
	if (playerRuntimeController_ && playerRuntimeController_->IsActive()) return;
	K4E::Stage* stage = K4E::Stage::GetActiveRuntimeStage();
	if (!stage) return;
	if (!playerRuntimeController_) playerRuntimeController_ = std::make_unique<GamePlayPlayerMigrationRuntime>();
	if (!playerRuntimeController_->Initialize(ctx_.bulletManager_, stage, &actorWorld_, &physicsWorld_, playerSpawnPosition_)) return;
	RegisterPlayerCollisionBridge();
	MidRangeBombProjectile::SetTargetPlayerRuntime(GetPlayerRuntime()); // 中距離Bombも旧Player Owner判定を経由せず同じRuntime正本へDamageを適用する。
}

void CharacterWorld::RegisterPlayerCollisionBridge()
{
	if (playerCollisionBridgeRegistered_ || !ctx_.collisionManager_) return;
	IPlayerRuntime* player = GetPlayerRuntime();
	K4E::Collider* collider = player ? player->GetCollisionPrimitive() : nullptr;
	if (!collider) return;
	ctx_.collisionManager_->AddCollider(collider);
	playerCollisionBridgeRegistered_ = true;
}

void CharacterWorld::UnregisterPlayerCollisionBridge()
{
	if (!playerCollisionBridgeRegistered_ || !ctx_.collisionManager_)
	{
		playerCollisionBridgeRegistered_ = false;
		return;
	}
	IPlayerRuntime* player = GetPlayerRuntime();
	if (player && player->GetCollisionPrimitive()) ctx_.collisionManager_->RemoveCollider(player->GetCollisionPrimitive());
	playerCollisionBridgeRegistered_ = false;
}

void CharacterWorld::UpdateActivePlayer(float deltaTime)
{
	EnsurePlayerRuntime();
	if (playerRuntimeController_ && playerRuntimeController_->IsActive()) playerRuntimeController_->Update(deltaTime);
}

void CharacterWorld::InjectEnemyDeps(EnemyBase& enemy)
{
	if (dynamic_cast<MidRangeEnemy*>(&enemy) == nullptr) enemy.SetParticleEffectSystem(&enemyParticleEffectSystem_);

	EnsurePlayerRuntime();
	K4E::Collider* playerCollider = GetPlayerRuntime() ? GetPlayerRuntime()->GetCollisionPrimitive() : nullptr;
	if (auto* meleeEnemy = dynamic_cast<MeleeEnemy*>(&enemy))
	{
		meleeEnemy->SetTarget(playerCollider);
	}
	else if (auto* midRangeEnemy = dynamic_cast<MidRangeEnemy*>(&enemy))
	{
		midRangeEnemy->SetTarget(playerCollider);
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
		ctx_.collisionManager_->AddCollider(enemy->GetCollisionPrimitive());
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
	EnsurePlayerRuntime();
	if (playerRuntimeController_ && playerRuntimeController_->IsActive()) playerRuntimeController_->Update(0.0f, false);
	for (auto& enemy : enemies_) if (enemy) enemy->Update(0.0f);
}

void CharacterWorld::SetStartGameplayVisualsVisible(bool visible)
{
	(void)visible; // 新Playerは一人称Weapon Viewを通常Runtime側で管理し、旧Player表示切替には依存しない。
}

void CharacterWorld::Draw()
{
	if (playerRuntimeController_ && playerRuntimeController_->IsActive()) actorWorld_.Draw();
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
	ImGui::SeparatorText("P13 Player Runtime");
	ImGui::Text("Player Runtime: %s", playerRuntimeController_ && playerRuntimeController_->IsActive() ? "ACTIVE" : "WAITING");
	ImGui::Text("Legacy Player Instance: NONE");
	ImGui::Text("ActorWorld Owned Actors: %d", static_cast<int>(actorWorld_.GetActors().size()));
	if (playerRuntimeController_)
	{
		ImGui::Text("Stage Colliders: %d / %d active",
			static_cast<int>(playerRuntimeController_->GetRegisteredStageColliderCount()),
			static_cast<int>(playerRuntimeController_->GetTotalStageColliderCount()));
	}
	if (const K4E::PlayerActor* player = GetPlayer())
	{
		const K4E::Vector3 position = player->GetWorldPosition();
		ImGui::Text("Player Pos: %.2f, %.2f, %.2f", position.x, position.y, position.z);
		ImGui::Text("Player HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
		for (const auto& componentOwner : player->GetComponents())
		{
			if (!componentOwner) continue;
			K4E::ActorComponent* component = componentOwner.get();
			const std::string label = component->GetName().empty() ? component->GetClassTypeName() : component->GetName();
			ImGui::PushID(component);
			if (ImGui::CollapsingHeader(label.c_str())) component->DrawImGui();
			ImGui::PopID();
		}
	}
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

	if (const K4E::PlayerActor* player = GetPlayer(); player && ImGui::Button("選択した敵を生成"))
	{
		const Vector3 spawnPosition = player->GetWorldPosition() + Vector3{ 0.0f, 0.0f, 3.0f };
		SpawnEnemyAt(spawnPosition, debugSpawnEnemyType_);
	}
	ImGui::Separator();
#endif
}
