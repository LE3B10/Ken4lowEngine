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
		SanitizeThresholds();
		currentPhase_ = std::clamp(currentPhase_, 1, 3);
	}

	void BossPhaseComponent::Update(float deltaTime)
	{
		(void)deltaTime;
		const auto* owner = dynamic_cast<const CharacterActor*>(GetOwner());
		const CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr;
		if (!health || health->IsDead()) return;

		const int evaluated = EvaluatePhase(health->GetHealthRatio());
		if (evaluated <= currentPhase_) return; // 回復しても戦闘フェーズは巻き戻さない。
		currentPhase_ = evaluated;
		++phaseRevision_;
	}

	void BossPhaseComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボスフェーズ");
		ImGui::Text("現在: Phase %d / Revision %u", currentPhase_, phaseRevision_);
		ImGui::Text("Phase 2: HP %.0f%% / Phase 3: HP %.0f%%", phase2HealthRatio_ * 100.0f, phase3HealthRatio_ * 100.0f);
#endif
	}

	void BossPhaseComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["Phase2HealthRatio"] = phase2HealthRatio_;
		outJson["Phase3HealthRatio"] = phase3HealthRatio_;
		outJson["CurrentPhase"] = currentPhase_;
	}

	void BossPhaseComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		phase2HealthRatio_ = inJson.value("Phase2HealthRatio", phase2HealthRatio_);
		phase3HealthRatio_ = inJson.value("Phase3HealthRatio", phase3HealthRatio_);
		currentPhase_ = inJson.value("CurrentPhase", currentPhase_);
		SanitizeThresholds();
		currentPhase_ = std::clamp(currentPhase_, 1, 3);
	}

	void BossPhaseComponent::ResetPhase()
	{
		currentPhase_ = 1;
		++phaseRevision_; // PresentationへReset後の状態再同期を通知する。
	}

	int BossPhaseComponent::EvaluatePhase(float healthRatio) const
	{
		if (healthRatio <= phase3HealthRatio_) return 3;
		if (healthRatio <= phase2HealthRatio_) return 2;
		return 1;
	}

	void BossPhaseComponent::SanitizeThresholds()
	{
		phase2HealthRatio_ = std::isfinite(phase2HealthRatio_) ? std::clamp(phase2HealthRatio_, 0.01f, 1.0f) : 0.70f;
		phase3HealthRatio_ = std::isfinite(phase3HealthRatio_) ? std::clamp(phase3HealthRatio_, 0.0f, phase2HealthRatio_) : 0.35f;
	}
} // namespace Ken4lowEngine
