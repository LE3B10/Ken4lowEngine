#include "EnemyMigrationValidation.h"

#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"
#include "ApplicationLayer/Character/Enemy/Core/MeleeEnemy.h"
#include "ApplicationLayer/Character/Enemy/Effects/EnemyParticleEffectSystem.h"

#include <ActorWorld.h>
#include <Collider.h>
#include <SceneComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	const K4E::Vector3 kLegacyEnemyPosition{ -8.0f, 2.0f, 9.0f };
	const K4E::Vector3 kLegacyTargetPosition{ -8.0f, 2.0f, 0.0f };
	const K4E::Vector3 kComponentEnemyPosition{ 8.0f, 2.0f, 9.0f };
	const K4E::Vector3 kComponentTargetPosition{ 8.0f, 2.0f, 0.0f };

	/// 比較表の浮動小数値が実用上同じか判定する。
	bool NearlyEqual(float lhs, float rhs)
	{
		return std::abs(lhs - rhs) <= 0.001f;
	}

#ifdef USE_IMGUI
	/// 比較結果をOK/差分付きで表の最終列へ表示する。
	void DrawComparisonResult(bool matched)
	{
		ImGui::TextColored(matched ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
			matched ? "OK" : "確認");
	}
#endif
}

EnemyMigrationValidation::~EnemyMigrationValidation() = default;

void EnemyMigrationValidation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	navigationObstacles_ = {
		{ { -9.0f, 0.0f, 4.0f }, { -7.0f, 3.0f, 6.0f } },
		{ { 7.0f, 0.0f, 4.0f }, { 9.0f, 3.0f, 6.0f } },
	};

	legacyEffectSystem_ = std::make_unique<EnemyParticleEffectSystem>();
	legacyEffectSystem_->Initialize();
	legacyTarget_ = std::make_unique<K4E::Collider>();
	legacyTarget_->SetCenterPosition(kLegacyTargetPosition);
	legacyTarget_->SetOBBHalfSize({ 0.5f, 1.0f, 0.5f });
	legacyEnemy_ = std::make_unique<MeleeEnemy>();
	legacyEnemy_->Initialize();
	legacyEnemy_->SetPosition(kLegacyEnemyPosition);
	legacyEnemy_->SetTarget(legacyTarget_.get());
	legacyEnemy_->SetWallObstacleAABBs(&navigationObstacles_);
	legacyEnemy_->SetParticleEffectSystem(legacyEffectSystem_.get());

	componentTarget_ = &actorWorld_->SpawnActor<K4E::CharacterActor>();
	componentTarget_->SetName("ComponentEnemyTarget");
	componentTarget_->SetLayer("EnemyComparison");
	if (K4E::SceneComponent* targetRoot = componentTarget_->GetRootComponent())
	{
		targetRoot->SetLocalPosition(kComponentTargetPosition);
		targetRoot->RefreshWorldTransform();
	}
	if (K4E::CharacterHealthComponent* health = componentTarget_->GetHealthComponent()) health->ResetHealth(10000.0f);

	componentEnemy_ = &actorWorld_->SpawnActor<K4E::EnemyActor>();
	componentEnemy_->SetName("ComponentEnemy");
	componentEnemy_->SetLayer("EnemyComparison");
	componentEnemy_->AddTag("NormalEnemy");
	componentEnemy_->SetTargetActor(componentTarget_);
	componentEnemy_->SetNavigationObstacles(&navigationObstacles_);
	componentEnemy_->ResetForComparison(kComponentEnemyPosition);
}

void EnemyMigrationValidation::Update(float deltaTime)
{
	ProcessRequests();
	if (legacyEnemy_) legacyEnemy_->Update(deltaTime); // 旧個体だけ手動更新し、新個体はActorWorld側で一度だけ更新する。
}

void EnemyMigrationValidation::UpdateEditor()
{
	ProcessRequests(); // Editor停止中は攻撃AIや移動積分を進めない。
}

void EnemyMigrationValidation::DrawLegacy()
{
	if (legacyEnemy_) legacyEnemy_->Draw();
}

void EnemyMigrationValidation::DrawLegacyShadow()
{
	if (legacyEnemy_) legacyEnemy_->DrawShadow();
}

void EnemyMigrationValidation::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("通常敵 Component 移行検証"))
	{
		ImGui::End();
		return;
	}

	const K4E::EnemyAIComponent* ai = componentEnemy_ ? componentEnemy_->GetEnemyAIComponent() : nullptr;
	const K4E::EnemyAttackComponent* attack = componentEnemy_ ? componentEnemy_->GetEnemyAttackComponent() : nullptr;
	const K4E::EnemyEffectComponent* effect = componentEnemy_ ? componentEnemy_->GetEnemyEffectComponent() : nullptr;
	const K4E::HumanoidVisualComponent* visual = componentEnemy_ ? componentEnemy_->GetHumanoidVisualComponent() : nullptr;
	const K4E::CharacterColliderComponent* collider = componentEnemy_ ? componentEnemy_->GetColliderComponent() : nullptr;
	const K4E::CharacterHealthComponent* health = componentEnemy_ ? componentEnemy_->GetHealthComponent() : nullptr;

	ImGui::TextUnformatted("左: 旧MeleeEnemy / 右: EnemyActor + Component");
	ImGui::TextUnformatted("同じNavigator・移動速度・Scratch攻撃値を使用し、各個体は片方の経路だけで動作します。");
	if (ImGui::BeginTable("EnemyComparisonTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("確認項目");
		ImGui::TableSetupColumn("旧通常敵");
		ImGui::TableSetupColumn("Component通常敵");
		ImGui::TableSetupColumn("結果");
		ImGui::TableHeadersRow();

		auto nextRow = [](const char* label)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(label);
		};

		nextRow("移動速度");
		const float legacyMoveSpeed = legacyEnemy_ ? legacyEnemy_->GetComparisonMoveSpeed() : 0.0f;
		ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", legacyMoveSpeed);
		ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", ai ? ai->GetMoveSpeed() : 0.0f);
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(ai && NearlyEqual(legacyMoveSpeed, ai->GetMoveSpeed()));

		nextRow("A*経路探索");
		ImGui::TableSetColumnIndex(1); ImGui::Text("%s / %zu nodes", legacyEnemy_ && legacyEnemy_->HasComparisonPath() ? "有効" : "待機", legacyEnemy_ ? legacyEnemy_->GetComparisonPathNodeCount() : 0u);
		ImGui::TableSetColumnIndex(2); ImGui::Text("%s / %zu nodes", ai && ai->HasPath() ? "有効" : "待機", ai ? ai->GetPathNodeCount() : 0u);
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(legacyEnemy_ && ai);

		nextRow("攻撃間隔");
		const float legacyCooldown = legacyEnemy_ ? legacyEnemy_->GetComparisonAttackCooldown() : 0.0f;
		const float legacyInterval = legacyEnemy_ ? legacyEnemy_->GetComparisonAttackInterval() : 0.0f;
		ImGui::TableSetColumnIndex(1); ImGui::Text("実 %.2f / CD %.2f sec", legacyInterval, legacyCooldown);
		ImGui::TableSetColumnIndex(2); ImGui::Text("実 %.2f / CD %.2f / hit %d", attack ? attack->GetExpectedHitInterval() : 0.0f, attack ? attack->GetAttackCooldown() : 0.0f, attack ? attack->GetAcceptedHitCount() : 0);
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(attack && NearlyEqual(legacyInterval, attack->GetExpectedHitInterval()) && NearlyEqual(legacyCooldown, attack->GetAttackCooldown()));

		nextRow("攻撃ダメージ");
		const int legacyDamage = legacyEnemy_ ? legacyEnemy_->GetComparisonAttackDamage() : 0;
		ImGui::TableSetColumnIndex(1); ImGui::Text("%d", legacyDamage);
		ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f", attack ? attack->GetAttackDamage() : 0.0f);
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(attack && NearlyEqual(static_cast<float>(legacyDamage), attack->GetAttackDamage()));

		nextRow("HP / 死亡処理");
		ImGui::TableSetColumnIndex(1); ImGui::Text("%d/%d / %s", legacyEnemy_ ? legacyEnemy_->GetHp() : 0, legacyEnemy_ ? legacyEnemy_->GetMaxHp() : 0, legacyEnemy_ && legacyEnemy_->IsDead() ? "死亡" : "生存");
		ImGui::TableSetColumnIndex(2); ImGui::Text("%.0f/%.0f / %s", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f, componentEnemy_ && componentEnemy_->IsDead() ? "死亡" : "生存");
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(legacyEnemy_ && componentEnemy_ && legacyEnemy_->IsDead() == componentEnemy_->IsDead());

		nextRow("Effect");
		ImGui::TableSetColumnIndex(1); ImGui::Text("Hit %d / Death %d", legacyHitEffectCount_, legacyDeathEffectCount_);
		ImGui::TableSetColumnIndex(2); ImGui::Text("Hit %d / Death %d", effect ? effect->GetHitEffectCount() : 0, effect ? effect->GetDeathEffectCount() : 0);
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(effect && legacyHitEffectCount_ == effect->GetHitEffectCount() && legacyDeathEffectCount_ == effect->GetDeathEffectCount());

		nextRow("Collider");
		const K4E::Vector3 legacyHalf = legacyEnemy_ ? legacyEnemy_->GetOBBHalfSize() : K4E::Vector3{};
		const K4E::Vector3 componentHalf = collider ? collider->GetHalfSize() : K4E::Vector3{};
		const bool legacyColliderEnabled = legacyEnemy_ && legacyEnemy_->IsEnabled();
		const bool componentColliderEnabled = collider && collider->IsActive();
		const K4E::SceneComponent* componentRoot = componentEnemy_ ? componentEnemy_->GetRootComponent() : nullptr;
		const K4E::Collider* runtimeCollider = collider ? collider->GetCollider() : nullptr;
		const bool colliderFollowsRoot = componentRoot && runtimeCollider &&
			K4E::Vector3::Length(runtimeCollider->GetCenterPosition() - componentRoot->GetWorldPosition()) <= 0.001f;
		const bool colliderSizeMatched = NearlyEqual(legacyHalf.x, componentHalf.x) && NearlyEqual(legacyHalf.y, componentHalf.y) && NearlyEqual(legacyHalf.z, componentHalf.z);
		ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f, %.1f, %.1f / %s", legacyHalf.x, legacyHalf.y, legacyHalf.z, legacyColliderEnabled ? "有効" : "停止");
		ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f, %.1f, %.1f / %s / 追従%s", componentHalf.x, componentHalf.y, componentHalf.z, componentColliderEnabled ? "有効" : "停止", colliderFollowsRoot ? "OK" : "確認");
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(legacyColliderEnabled == componentColliderEnabled && (!legacyColliderEnabled || (colliderSizeMatched && colliderFollowsRoot)));

		nextRow("描画 / Shadow");
		const size_t legacyPartCount = legacyEnemy_ ? legacyEnemy_->GetBodyParts().size() + 1u : 0u;
		const size_t componentPartCount = visual ? visual->GetParts().size() : 0u;
		ImGui::TableSetColumnIndex(1); ImGui::Text("%zu parts / Shadow", legacyPartCount);
		ImGui::TableSetColumnIndex(2); ImGui::Text("%zu parts / %s", componentPartCount, visual && visual->IsCastShadowEnabled() ? "Shadow" : "Off");
		ImGui::TableSetColumnIndex(3); DrawComparisonResult(visual && visual->IsCastShadowEnabled() && legacyPartCount == componentPartCount);

		ImGui::EndTable();
	}

	if (ImGui::Button("両方へ60ダメージ")) requestDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("両方を死亡させる")) requestLethalDamage_ = true;
	ImGui::TextUnformatted("死亡後の再比較はSceneを再読み込みしてください。GPU描画中の旧モデル再生成を避けています。");
	ImGui::End();
#endif
}

void EnemyMigrationValidation::Finalize()
{
	legacyEnemy_.reset();
	legacyTarget_.reset();
	legacyEffectSystem_.reset();
	componentEnemy_ = nullptr;
	componentTarget_ = nullptr;
	actorWorld_ = nullptr;
	navigationObstacles_.clear();
}

void EnemyMigrationValidation::ProcessRequests()
{
	if (requestDamage_)
	{
		requestDamage_ = false;
		ApplyDamageToBoth(60);
	}
	if (requestLethalDamage_)
	{
		requestLethalDamage_ = false;
		ApplyDamageToBoth(10000);
	}
}

void EnemyMigrationValidation::ApplyDamageToBoth(int amount)
{
	if (amount <= 0) return;
	if (legacyEnemy_ && !legacyEnemy_->IsDead())
	{
		const K4E::Vector3 hitPosition = legacyEnemy_->GetCenterPosition();
		legacyEnemy_->SpawnHitEffectAt(hitPosition);
		++legacyHitEffectCount_;
		legacyEnemy_->TakeDamage(amount, { 0.0f, 0.0f, -1.0f }, 3.0f);
		if (legacyEnemy_->IsDead()) ++legacyDeathEffectCount_;
	}
	if (componentEnemy_ && !componentEnemy_->IsDead()) componentEnemy_->ApplyComparisonDamage(static_cast<float>(amount));
}
