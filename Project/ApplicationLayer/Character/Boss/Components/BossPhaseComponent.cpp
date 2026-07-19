#include "BossPhaseComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossPhaseComponent::Initialize()
	{
		SanitizeSettings();
		currentPhase_ = std::clamp(currentPhase_, 1, 3);
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}

	void BossPhaseComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr;
		if (!health) return;

		UpdatePhaseInvulnerability(*health, deltaTime);
		if (health->IsDead() || phaseInvulnerabilityActive_) return;

		const int evaluated = EvaluatePhase(health->GetHealthRatio());
		if (evaluated <= currentPhase_) return; // 回復しても戦闘フェーズは巻き戻さない。

		currentPhase_ = std::min(evaluated, currentPhase_ + 1); // 大ダメージでもPhase 2とPhase 3を順番に見せる。
		++phaseRevision_;
		BeginPhaseInvulnerability(*health);
	}

	void BossPhaseComponent::Finalize()
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		if (CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr)
		{
			if (phaseInvulnerabilityActive_) EndPhaseInvulnerability(*health);
		}
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}

	void BossPhaseComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボスフェーズ");
		ImGui::Text("現在: Phase %d / Revision %u", currentPhase_, phaseRevision_);
		ImGui::Text("Phase 2: HP %.0f%% / Phase 3: HP %.0f%%", phase2HealthRatio_ * 100.0f, phase3HealthRatio_ * 100.0f);
		ImGui::Text("移行無敵: %s %.2f / %.2f", phaseInvulnerabilityActive_ ? "有効" : "無効", phaseInvulnerabilityRemaining_, phaseInvulnerabilityDuration_);
#endif
	}

	void BossPhaseComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["Phase2HealthRatio"] = phase2HealthRatio_;
		outJson["Phase3HealthRatio"] = phase3HealthRatio_;
		outJson["PhaseInvulnerabilityDuration"] = phaseInvulnerabilityDuration_;
		outJson["CurrentPhase"] = currentPhase_;
	}

	void BossPhaseComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		phase2HealthRatio_ = inJson.value("Phase2HealthRatio", phase2HealthRatio_);
		phase3HealthRatio_ = inJson.value("Phase3HealthRatio", phase3HealthRatio_);
		phaseInvulnerabilityDuration_ = inJson.value("PhaseInvulnerabilityDuration", phaseInvulnerabilityDuration_);
		currentPhase_ = inJson.value("CurrentPhase", currentPhase_);
		SanitizeSettings();
		currentPhase_ = std::clamp(currentPhase_, 1, 3);
	}

	void BossPhaseComponent::ResetPhase()
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		if (CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr)
		{
			if (phaseInvulnerabilityActive_) EndPhaseInvulnerability(*health);
		}
		currentPhase_ = 1;
		phaseInvulnerabilityRemaining_ = 0.0f;
		++phaseRevision_; // PresentationへReset後の状態再同期を通知する。
	}

	int BossPhaseComponent::EvaluatePhase(float healthRatio) const
	{
		if (healthRatio <= phase3HealthRatio_) return 3;
		if (healthRatio <= phase2HealthRatio_) return 2;
		return 1;
	}

	void BossPhaseComponent::SanitizeSettings()
	{
		phase2HealthRatio_ = std::isfinite(phase2HealthRatio_) ? std::clamp(phase2HealthRatio_, 0.01f, 1.0f) : 0.70f;
		phase3HealthRatio_ = std::isfinite(phase3HealthRatio_) ? std::clamp(phase3HealthRatio_, 0.0f, phase2HealthRatio_) : 0.35f;
		phaseInvulnerabilityDuration_ = std::isfinite(phaseInvulnerabilityDuration_)
			? std::clamp(phaseInvulnerabilityDuration_, 0.10f, 5.0f)
			: 1.20f;
	}

	void BossPhaseComponent::BeginPhaseInvulnerability(CharacterHealthComponent& health)
	{
		wasInvulnerableBeforePhase_ = health.IsInvulnerable();
		phaseInvulnerabilityRemaining_ = phaseInvulnerabilityDuration_;
		phaseInvulnerabilityActive_ = true;
		health.SetInvulnerable(true); // フェーズ演出と次の行動準備中に連射で削り切られないようDamageを遮断する。
	}

	void BossPhaseComponent::UpdatePhaseInvulnerability(CharacterHealthComponent& health, float deltaTime)
	{
		if (!phaseInvulnerabilityActive_) return;
		health.SetInvulnerable(true);
		if (std::isfinite(deltaTime) && deltaTime > 0.0f)
		{
			phaseInvulnerabilityRemaining_ = std::max(0.0f, phaseInvulnerabilityRemaining_ - deltaTime);
		}
		if (phaseInvulnerabilityRemaining_ <= 0.0f || health.IsDead()) EndPhaseInvulnerability(health);
	}

	void BossPhaseComponent::EndPhaseInvulnerability(CharacterHealthComponent& health)
	{
		health.SetInvulnerable(wasInvulnerableBeforePhase_);
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}
} // namespace Ken4lowEngine
