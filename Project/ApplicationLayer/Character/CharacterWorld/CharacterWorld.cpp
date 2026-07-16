#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "BulletManager.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"

#include <Stage.h>

#include <algorithm>
#include <cmath>

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
	actorWorld_.Initialize(); // Playerと通常Enemyを同じActorWorld・PhysicsWorldへ接続する。

	playerSpawnPosition_ = { 0.0f, 2.25f, 0.0f };
	playerCollisionBridgeRegistered_ = false;
	enemyActors_.clear();
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
	MidRangeBombProjectile::SetTargetPlayerRuntime(GetPlayerRuntime()); // 中距離Bombは旧Player Owner判定に依存せずRuntime正本へDamageを適用する。
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

void CharacterWorld::InjectEnemyDeps(K4E::EnemyActor& enemy)
{
	enemy.SetParticleEffectSystem(&enemyParticleEffectSystem_);
	EnsurePlayerRuntime();
	enemy.SetTargetActor(GetPlayer());
	if (K4E::Stage* stage = K4E::Stage::GetActiveRuntimeStage())
	{
		enemy.SetNavigationObstacles(&stage->GetNavigationObstacleAABBs()); // 両アーキタイプのA* Componentへ同じStage障害物参照を渡す。
	}
}

std::vector<EnemyBase*> CharacterWorld::GetEnemyRawList() const
{
	std::vector<EnemyBase*> result;
	result.reserve(enemyActors_.size());
	for (K4E::EnemyActor* enemy : enemyActors_)
	{
		if (enemy) result.push_back(enemy);
	}
	return result;
}

EnemyBase& CharacterWorld::SpawnEnemy(const EnemySpawnRequest& request)
{
	EnsurePlayerRuntime();
	K4E::EnemyActor& enemy = actorWorld_.SpawnActor<K4E::EnemyActor>(request.enemyType);
	InjectEnemyDeps(enemy);
	enemy.SetPosition(request.position);
	enemy.SetOrientation({ 0.0f, request.yawRad, 0.0f });
	if (request.maxHp > 0.0f) enemy.SetMaxHp(std::max(1, static_cast<int>(std::round(request.maxHp))));

	if (ctx_.collisionManager_ && enemy.GetCollisionPrimitive())
	{
		ctx_.collisionManager_->AddCollider(enemy.GetCollisionPrimitive()); // PhysicsWorld所有とは別にLegacy弾・照準用Bridgeだけを登録する。
	}

	enemyActors_.push_back(&enemy);
	++spawnedEnemyCounts_[ToEnemyTypeIndex(request.enemyType)];
	return enemy;
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
	for (const K4E::EnemyActor* enemy : enemyActors_)
	{
		if (enemy && !enemy->IsDead()) ++aliveCount;
	}
	return aliveCount;
}

void CharacterWorld::ClearEnemies()
{
	notifiedKilledEnemies_.clear();
	for (K4E::EnemyActor* enemy : enemyActors_)
	{
		if (!enemy) continue;
		if (ctx_.collisionManager_ && enemy->GetCollisionPrimitive()) ctx_.collisionManager_->RemoveCollider(enemy->GetCollisionPrimitive());
		actorWorld_.DestroyActor(enemy);
	}
	enemyActors_.clear();
}

bool CharacterWorld::RemoveEnemy(EnemyBase* enemy)
{
	auto* actorEnemy = dynamic_cast<K4E::EnemyActor*>(enemy);
	if (!actorEnemy) return false;
	const auto it = std::find(enemyActors_.begin(), enemyActors_.end(), actorEnemy);
	if (it == enemyActors_.end()) return false;

	notifiedKilledEnemies_.erase(actorEnemy);
	if (ctx_.collisionManager_ && actorEnemy->GetCollisionPrimitive()) ctx_.collisionManager_->RemoveCollider(actorEnemy->GetCollisionPrimitive());
	actorWorld_.DestroyActor(actorEnemy);
	enemyActors_.erase(it);
	return true;
}

void CharacterWorld::Update(float deltaTime)
{
	UpdateActivePlayer(deltaTime); // Player Runtimeが共有ActorWorld・PhysicsWorldを一度だけ更新する。

	for (K4E::EnemyActor* enemy : enemyActors_)
	{
		if (!enemy) continue;
		const bool wasAlreadyNotified = notifiedKilledEnemies_.contains(enemy);
		if (!wasAlreadyNotified && enemy->IsDead())
		{
			notifiedKilledEnemies_.insert(enemy);
			if (onEnemyKilled_) onEnemyKilled_(enemy->GetCenterPosition());
		}
	}

	std::erase_if(enemyActors_, [&](K4E::EnemyActor* enemy)
		{
			if (!enemy || !enemy->IsRemovable()) return false;
			notifiedKilledEnemies_.erase(enemy);
			if (ctx_.collisionManager_ && enemy->GetCollisionPrimitive()) ctx_.collisionManager_->RemoveCollider(enemy->GetCollisionPrimitive());
			actorWorld_.DestroyActor(enemy); // ActorWorldがPhysics登録解除と実体破棄を安全なフレーム境界で行う。
			return true;
		});
}

void CharacterWorld::UpdatePlayerOnly(float deltaTime)
{
	for (K4E::EnemyActor* enemy : enemyActors_) if (enemy) enemy->SetSimulationEnabled(false);
	UpdateActivePlayer(deltaTime);
	for (K4E::EnemyActor* enemy : enemyActors_) if (enemy) enemy->SetSimulationEnabled(true); // Intro・装備演出中はPlayerだけを進め、敵のAI時間を消費しない。
}

void CharacterWorld::WarmupStartGameplayVisuals()
{
	EnsurePlayerRuntime();
	if (playerRuntimeController_ && playerRuntimeController_->IsActive()) playerRuntimeController_->Update(0.0f, false);
}

void CharacterWorld::SetStartGameplayVisualsVisible(bool visible)
{
	(void)visible; // PlayerとEnemyの表示は各Actor Componentが管理し、旧Player表示切替には依存しない。
}

void CharacterWorld::Draw()
{
	actorWorld_.Draw();
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
	for (const K4E::EnemyActor* enemy : enemyActors_)
	{
		if (!enemy) continue;
		totalStuckDetections += enemy->GetStuckDetectionCount();
		totalStuckRecoveries += enemy->GetStuckRecoveryCount();
	}
	ImGui::Text("スタック検出回数: %d", totalStuckDetections);
	ImGui::Text("スタック復帰回数: %d", totalStuckRecoveries);
	if (!enemyActors_.empty() && enemyActors_.front())
	{
		const Vector3 position = enemyActors_.front()->GetCenterPosition();
		ImGui::Text("敵の現在座標: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
	}

	std::array<int, 2> liveEnemyCounts{};
	for (const K4E::EnemyActor* enemy : enemyActors_)
	{
		if (enemy) ++liveEnemyCounts[ToEnemyTypeIndex(enemy->GetEnemyType())];
	}

	ImGui::Text("現在の敵数: %d", static_cast<int>(enemyActors_.size()));
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
