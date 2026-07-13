#include "EnemyAttackComponent.h"

#include "EnemyAIComponent.h"

#include <Scene/Actor/Character/AttackBehaviors.h>
#include <Scene/Actor/Character/CharacterActor.h>

#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void EnemyAttackComponent::Initialize()
	{
		EnsureDefaultMeleeAttack();
		AttackComponent::Initialize(); // タイマー、イベント、攻撃Phaseは共通基盤だけで初期化する。
	}

	void EnemyAttackComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		const auto* ai = owner ? owner->GetCharacterComponent<EnemyAIComponent>() : nullptr;
		if (owner && !owner->IsDead() && ai && GetTargetActor() && !GetTargetActor()->IsDead()
			&& ai->GetDistanceToTarget() <= ai->GetAttackStartRange())
		{
			StartAttack("Melee"); // Adapterは開始判断だけを行い、Damageや腕Transformを直接処理しない。
		}
		AttackComponent::Update(deltaTime);
	}

	void EnemyAttackComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵攻撃");
		ImGui::Text("Damage: %.1f / Cooldown: %.2f / 命中間隔: %.2f", GetAttackDamage(), GetAttackCooldown(), GetExpectedHitInterval());
		ImGui::Text("命中回数: %d / 実測間隔: %.3f", GetAcceptedHitCount(), GetLastMeasuredInterval());
#endif
	}

	void EnemyAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		AttackComponent::ToJson(outJson);
		const AttackData* melee = FindAttackData("Melee");
		if (!melee) return;
		outJson["AttackCooldown"] = melee->cooldown;
		outJson["AttackDamage"] = melee->damage;
		outJson["AttackStartDelay"] = melee->windupTime;
		outJson["AttackActiveTime"] = melee->activeTime;
		outJson["AttackRecoveryTime"] = melee->recoveryTime; // Phase 7のJSONキーも残し、既存Actorデータを読み戻せるようにする。
	}

	void EnemyAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		AttackComponent::FromJson(inJson);
		EnsureDefaultMeleeAttack();
		const AttackData* restored = FindAttackData("Melee");
		if (!restored) return;
		AttackData melee = *restored;
		melee.cooldown = std::max(0.05f, inJson.value("AttackCooldown", melee.cooldown));
		melee.damage = std::max(0.0f, inJson.value("AttackDamage", melee.damage));
		melee.windupTime = std::max(0.0f, inJson.value("AttackStartDelay", melee.windupTime));
		melee.activeTime = std::max(0.0f, inJson.value("AttackActiveTime", melee.activeTime));
		melee.recoveryTime = std::max(0.0f, inJson.value("AttackRecoveryTime", melee.recoveryTime));
		ConfigureAttack("Melee", melee);
	}

	float EnemyAttackComponent::GetAttackCooldown() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->cooldown : 0.0f;
	}

	float EnemyAttackComponent::GetExpectedHitInterval() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->windupTime + melee->activeTime + melee->recoveryTime + melee->cooldown : 0.0f;
	}

	float EnemyAttackComponent::GetAttackDamage() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->damage : 0.0f;
	}

	void EnemyAttackComponent::EnsureDefaultMeleeAttack()
	{
		if (FindAttackData("Melee")) return;
		AttackData melee{};
		melee.id = "Melee";
		melee.behaviorType = "Melee";
		melee.animationName = "Attack.Melee";
		melee.damage = 8.0f;
		melee.cooldown = 0.55f;
		melee.windupTime = 0.12f;
		melee.activeTime = 0.10f;
		melee.recoveryTime = 0.35f;
		melee.maxRange = 2.4f;
		RegisterAttack(std::move(melee), std::make_unique<MeleeAttackBehavior>()); // 通常敵固有値だけをAdapterで登録し、実行ロジックは共通クラスを使う。
	}
} // namespace Ken4lowEngine
