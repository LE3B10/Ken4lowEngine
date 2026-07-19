#define NOMINMAX
#include "BossBrainComponent.h"

#include "BossActorAttackComponent.h"
#include "BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossBrainComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterMovementComponent* movement = owner ? owner->GetMovementComponent() : nullptr;
		auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr;
		const auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		const auto* presentation = owner ? owner->GetComponent<BossPresentationComponent>() : nullptr;
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		if (!owner || !movement || !attack || !root || !behaviorEnabled_ || owner->IsDead())
		{
			if (movement) movement->Stop();
			stateName_ = owner && owner->IsDead() ? "Dead" : "Disabled";
			return;
		}

		if (!targetActor_ || targetActor_->IsDead())
		{
			movement->Stop();
			stateName_ = "NoTarget";
			return;
		}

		observedPhase_ = phase ? phase->GetCurrentPhase() : 1;
		const float phaseMoveMultiplier = observedPhase_ >= 3 ? 1.45f : (observedPhase_ == 2 ? 1.15f : 1.0f);
		const float phaseRotateMultiplier = observedPhase_ >= 3 ? 1.35f : (observedPhase_ == 2 ? 1.10f : 1.0f);
		appliedMoveSpeed_ = moveSpeed_ * phaseMoveMultiplier;
		appliedRotateSpeed_ = rotateSpeed_ * phaseRotateMultiplier;

		const Vector3 current = root->GetWorldPosition();
		const Vector3 toTarget = targetActor_->GetTargetPosition() - current;
		distanceToTarget_ = Vector3::LengthXZ(toTarget);
		movement->FaceDirectionXZ(toTarget, appliedRotateSpeed_, deltaTime);

		if (presentation && presentation->IsBlockingBehavior())
		{
			movement->Stop();
			stateName_ = "Presentation";
			return;
		}
		if (attack->IsAttacking())
		{
			movement->Stop();
			stateName_ = "Attack";
			return;
		}

		if (attack->TryStartBestAttack(distanceToTarget_, observedPhase_))
		{
			movement->Stop();
			stateName_ = "Attack";
			return;
		}

		const float phaseApproachDistance = observedPhase_ >= 3 ? approachDistance_ * 0.82f : approachDistance_;
		if (distanceToTarget_ > phaseApproachDistance)
		{
			const float length = std::max(distanceToTarget_, 0.0001f);
			movement->SetVelocity({
				toTarget.x / length * appliedMoveSpeed_,
				0.0f,
				toTarget.z / length * appliedMoveSpeed_
			});
			stateName_ = "Approach";
			return;
		}

		movement->Stop();
		stateName_ = "Idle";
	}

	void BossBrainComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボス行動判断");
		ImGui::Text("状態: %s / Phase: %d / Target距離: %.2f", stateName_.c_str(), observedPhase_, distanceToTarget_);
		ImGui::Text("移動: %.2f -> %.2f / 旋回: %.2f -> %.2f", moveSpeed_, appliedMoveSpeed_, rotateSpeed_, appliedRotateSpeed_);
		ImGui::Text("接近停止: %.2f", approachDistance_);
#endif
	}

	void BossBrainComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["MoveSpeed"] = moveSpeed_;
		outJson["RotateSpeed"] = rotateSpeed_;
		outJson["ApproachDistance"] = approachDistance_;
	}

	void BossBrainComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		moveSpeed_ = std::max(0.0f, inJson.value("MoveSpeed", moveSpeed_));
		rotateSpeed_ = std::max(0.0f, inJson.value("RotateSpeed", rotateSpeed_));
		approachDistance_ = std::max(0.0f, inJson.value("ApproachDistance", approachDistance_));
		if (!std::isfinite(moveSpeed_)) moveSpeed_ = 2.6f;
		if (!std::isfinite(rotateSpeed_)) rotateSpeed_ = 5.5f;
		if (!std::isfinite(approachDistance_)) approachDistance_ = 2.8f;
	}

	void BossBrainComponent::StopBehavior()
	{
		behaviorEnabled_ = false;
		if (auto* owner = dynamic_cast<CharacterActor*>(GetOwner()))
		{
			if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		}
		stateName_ = "Stopped";
	}

	void BossBrainComponent::ResetBehavior()
	{
		behaviorEnabled_ = true;
		distanceToTarget_ = 0.0f;
		appliedMoveSpeed_ = moveSpeed_;
		appliedRotateSpeed_ = rotateSpeed_;
		observedPhase_ = 1;
		stateName_ = "Idle"; // 再戦時はPhase 1の速度表示へ戻し、前回の高速化状態を残さない。
	}
} // namespace Ken4lowEngine
