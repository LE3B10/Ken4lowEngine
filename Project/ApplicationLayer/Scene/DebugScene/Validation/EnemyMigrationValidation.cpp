#include "EnemyMigrationValidation.h"

#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"

#include <ActorWorld.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	const K4E::Vector3 kEnemyPosition{ 8.0f, 2.0f, 9.0f };
}

void EnemyMigrationValidation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	navigationObstacles_ = {
		{ { 7.0f, 0.0f, 4.0f }, { 9.0f, 3.0f, 6.0f } },
	};

	enemy_ = &actorWorld_->SpawnActor<K4E::EnemyActor>();
	enemy_->SetName(enemyName_);
	enemy_->SetLayer("EnemyValidation");
	enemy_->AddTag("NormalEnemy");
	enemy_->SetNavigationObstacles(&navigationObstacles_);
	enemy_->ResetForComparison(kEnemyPosition);

	RefreshActorReferencesAndBindings();
}

void EnemyMigrationValidation::Update(float deltaTime)
{
	(void)deltaTime;
	RefreshActorReferencesAndBindings();
	ProcessRequests();
}

void EnemyMigrationValidation::UpdateEditor()
{
	RefreshActorReferencesAndBindings();
	ProcessRequests();
}

void EnemyMigrationValidation::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("通常敵 Player Target 検証"))
	{
		ImGui::End();
		return;
	}

	const auto* ai = enemy_ ? enemy_->GetEnemyAIComponent() : nullptr;
	const auto* attack = enemy_ ? enemy_->GetEnemyAttackComponent() : nullptr;
	const auto* effect = enemy_ ? enemy_->GetEnemyEffectComponent() : nullptr;
	const auto* visual = enemy_ ? enemy_->GetHumanoidVisualComponent() : nullptr;
	const auto* collider = enemy_ ? enemy_->GetColliderComponent() : nullptr;
	const auto* health = enemy_ ? enemy_->GetHealthComponent() : nullptr;
	const auto* animation = enemy_ ? enemy_->GetAnimationComponent() : nullptr;
	const auto* rigidbodyComponent = enemy_ ? enemy_->GetComponent<K4E::RigidbodyComponent>() : nullptr;
	const auto* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;

	ImGui::Text("Target: %s", target_ ? target_->GetName().c_str() : "なし");
	ImGui::Text("AI: %s", ai ? (ai->HasPath() ? "追跡中" : "待機") : "None");
	ImGui::Text("Attack: %.0f damage / hit %d", attack ? attack->GetAttackDamage() : 0.0f, attack ? attack->GetAcceptedHitCount() : 0);
	ImGui::Text("HP: %.0f / %.0f", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f);
	ImGui::Text("Animation: %s", animation ? animation->GetAnimationName().c_str() : "None");
	ImGui::Text("Visual: %zu parts", visual ? visual->GetParts().size() : 0u);
	ImGui::Text("Collider: %s", collider && collider->IsActive() ? "有効" : "停止");
	ImGui::Text("Physics: %s / Grounded: %s", rigidbody ? "Dynamic" : "None", rigidbody && rigidbody->IsGrounded() ? "Yes" : "No");
	ImGui::Text("Effect: Hit %d / Death %d", effect ? effect->GetHitEffectCount() : 0, effect ? effect->GetDeathEffectCount() : 0);
	ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", lastMessage_.c_str());

	if (ImGui::Button("Enemyへ60ダメージ")) requestDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Enemyを死亡させる")) requestLethalDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Enemy Reset")) requestReset_ = true;
	ImGui::TextDisabled("旧MeleeEnemyと専用Target DummyはDebugSceneから削除済みです。");
	ImGui::End();
#endif
}

void EnemyMigrationValidation::Finalize()
{
	enemy_ = nullptr;
	target_ = nullptr;
	actorWorld_ = nullptr;
	navigationObstacles_.clear();
}

void EnemyMigrationValidation::RefreshActorReferencesAndBindings()
{
	if (!actorWorld_)
	{
		enemy_ = nullptr;
		target_ = nullptr;
		return;
	}

	enemy_ = dynamic_cast<K4E::EnemyActor*>(actorWorld_->FindActorByName(enemyName_));
	target_ = dynamic_cast<K4E::CharacterActor*>(actorWorld_->FindActorByName(targetName_));
	if (!enemy_)
	{
		lastSucceeded_ = false;
		lastMessage_ = "EnemyActorが見つかりません。";
		return;
	}

	enemy_->SetTargetActor(target_);
	enemy_->SetNavigationObstacles(&navigationObstacles_);
	lastSucceeded_ = target_ != nullptr;
	lastMessage_ = lastSucceeded_
		? "EnemyActorはDebugPlayerをTargetとして追跡・攻撃しています。"
		: "DebugPlayerが見つからないためTargetを解除しています。";
}

void EnemyMigrationValidation::ProcessRequests()
{
	if (requestDamage_)
	{
		requestDamage_ = false;
		if (enemy_ && !enemy_->IsDead()) enemy_->ApplyComparisonDamage(60.0f);
	}
	if (requestLethalDamage_)
	{
		requestLethalDamage_ = false;
		if (enemy_ && !enemy_->IsDead()) enemy_->ApplyComparisonDamage(10000.0f);
	}
	if (requestReset_)
	{
		requestReset_ = false;
		if (enemy_) enemy_->ResetForComparison(kEnemyPosition);
		RefreshActorReferencesAndBindings();
	}
}
