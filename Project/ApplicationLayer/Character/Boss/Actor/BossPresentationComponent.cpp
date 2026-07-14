#include "BossPresentationComponent.h"

#include "BossActorAttackComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossPresentationComponent::Initialize()
	{
		Actor* owner = GetOwner();
		const auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		observedPhaseRevision_ = phase ? phase->GetPhaseRevision() : 0;
		phaseTransitionDuration_ = std::isfinite(phaseTransitionDuration_) ? std::max(0.01f, phaseTransitionDuration_) : 0.8f;
		deathPresentationDuration_ = std::isfinite(deathPresentationDuration_) ? std::max(0.01f, deathPresentationDuration_) : 1.2f;
	}

	void BossPresentationComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		Actor* owner = GetOwner();
		auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		if (!deathPresentationActive_ && phase && phase->GetPhaseRevision() != observedPhaseRevision_)
		{
			observedPhaseRevision_ = phase->GetPhaseRevision();
			StartPhaseTransition(phase->GetCurrentPhase());
		}

		if (!phaseTransitionActive_ && !deathPresentationActive_) return;
		elapsed_ += deltaTime;
		const float duration = deathPresentationActive_ ? deathPresentationDuration_ : phaseTransitionDuration_;
		if (elapsed_ < duration) return;

		if (phaseTransitionActive_)
		{
			phaseTransitionActive_ = false;
			stateName_ = "Idle";
			if (auto* character = dynamic_cast<CharacterActor*>(owner))
			{
				if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Idle", 1.5f, true);
			}
		}
	}

	void BossPresentationComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボス演出");
		ImGui::Text("状態: %s / 経過: %.2f", stateName_.c_str(), elapsed_);
#endif
	}

	void BossPresentationComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["PhaseTransitionDuration"] = phaseTransitionDuration_;
		outJson["DeathPresentationDuration"] = deathPresentationDuration_;
	}

	void BossPresentationComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		phaseTransitionDuration_ = inJson.value("PhaseTransitionDuration", phaseTransitionDuration_);
		deathPresentationDuration_ = inJson.value("DeathPresentationDuration", deathPresentationDuration_);
		phaseTransitionDuration_ = std::isfinite(phaseTransitionDuration_) ? std::max(0.01f, phaseTransitionDuration_) : 0.8f;
		deathPresentationDuration_ = std::isfinite(deathPresentationDuration_) ? std::max(0.01f, deathPresentationDuration_) : 1.2f;
	}

	void BossPresentationComponent::StartDeathPresentation()
	{
		phaseTransitionActive_ = false;
		deathPresentationActive_ = true;
		elapsed_ = 0.0f;
		stateName_ = "Death";
		Actor* owner = GetOwner();
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->SetAttackEnabled(false);
		if (auto* character = dynamic_cast<CharacterActor*>(owner))
		{
			if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Boss.Dead", deathPresentationDuration_, false);
		}
	}

	void BossPresentationComponent::ResetPresentation()
	{
		phaseTransitionActive_ = false;
		deathPresentationActive_ = false;
		elapsed_ = 0.0f;
		stateName_ = "Idle";
		Actor* owner = GetOwner();
		const auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		observedPhaseRevision_ = phase ? phase->GetPhaseRevision() : 0;
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->SetAttackEnabled(true);
	}

	void BossPresentationComponent::StartPhaseTransition(int phase)
	{
		phaseTransitionActive_ = true;
		elapsed_ = 0.0f;
		stateName_ = "Phase " + std::to_string(phase);
		Actor* owner = GetOwner();
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->InterruptCurrentAttack();
		if (auto* character = dynamic_cast<CharacterActor*>(owner))
		{
			if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Boss.PhaseTransition", phaseTransitionDuration_, false);
		}
	}
} // namespace Ken4lowEngine
