#include "EnemyAttackComponent.h"

#include "EnemyAIComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void EnemyAttackComponent::Initialize()
	{
		ResetAttackState();
	}

	void EnemyAttackComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		elapsedSinceAcceptedHit_ += deltaTime;
		cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - deltaTime);

		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		const auto* ai = owner ? owner->GetCharacterComponent<EnemyAIComponent>() : nullptr;
		if (!owner || owner->IsDead() || !attackEnabled_ || !targetActor_ || targetActor_->IsDead() || !ai) return;
		if (ai->GetDistanceToTarget() > ai->GetAttackStartRange() || cooldownRemaining_ > 0.0f) return;

		const CharacterDamageResult result = targetActor_->ApplyDamage(attackDamage_);
		cooldownRemaining_ = GetExpectedHitInterval(); // 旧Scratchの予備・有効・復帰・Cooldownを含む実命中間隔を維持する。
		if (!result.accepted) return;

		lastMeasuredInterval_ = acceptedHitCount_ > 0 ? elapsedSinceAcceptedHit_ : 0.0f;
		elapsedSinceAcceptedHit_ = 0.0f;
		++acceptedHitCount_;
	}

	void EnemyAttackComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵攻撃");
		ImGui::Text("Damage: %.1f / Cooldown: %.2f / 命中間隔: %.2f", attackDamage_, attackCooldown_, GetExpectedHitInterval());
		ImGui::Text("命中回数: %d / 実測間隔: %.3f", acceptedHitCount_, lastMeasuredInterval_);
#endif
	}

	void EnemyAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["AttackCooldown"] = attackCooldown_;
		outJson["AttackDamage"] = attackDamage_;
		outJson["AttackStartDelay"] = attackStartDelay_;
		outJson["AttackActiveTime"] = attackActiveTime_;
		outJson["AttackRecoveryTime"] = attackRecoveryTime_;
	}

	void EnemyAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		attackCooldown_ = std::max(0.05f, inJson.value("AttackCooldown", attackCooldown_));
		attackDamage_ = std::max(0.0f, inJson.value("AttackDamage", attackDamage_));
		attackStartDelay_ = std::max(0.0f, inJson.value("AttackStartDelay", attackStartDelay_));
		attackActiveTime_ = std::max(0.0f, inJson.value("AttackActiveTime", attackActiveTime_));
		attackRecoveryTime_ = std::max(0.0f, inJson.value("AttackRecoveryTime", attackRecoveryTime_));
	}

	void EnemyAttackComponent::ResetAttackState()
	{
		attackEnabled_ = true;
		cooldownRemaining_ = attackStartDelay_; // 旧Scratch同様、範囲へ入った瞬間ではなく予備時間後に初回命中させる。
		elapsedSinceAcceptedHit_ = 0.0f;
		lastMeasuredInterval_ = 0.0f;
		acceptedHitCount_ = 0;
	}
} // namespace Ken4lowEngine
