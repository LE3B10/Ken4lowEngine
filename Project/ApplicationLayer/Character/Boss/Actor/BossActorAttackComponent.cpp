#include "BossActorAttackComponent.h"

#include <Scene/Actor/Character/AttackBehaviors.h>

#include <array>
#include <string_view>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossAttackComponent::Initialize()
	{
		RegisterDefaultAttacks();
		AttackComponent::Initialize();
	}

	void BossAttackComponent::DrawImGui()
	{
		AttackComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::Text("Boss選択: %s", lastSelectedAttackId_.c_str());
#endif
	}

	bool BossAttackComponent::TryStartBestAttack(float distanceToTarget, int bossPhase)
	{
		(void)distanceToTarget; // 最終的な水平距離と高さ差は共通AttackComponentがAttackDataから一元判定する。
		static constexpr std::array<std::string_view, 2> phase1{ "Punch", "HeavyPunch" };
		static constexpr std::array<std::string_view, 3> phase2{ "Charge", "HeavyPunch", "Punch" };
		static constexpr std::array<std::string_view, 4> phase3{ "Shockwave", "Charge", "HeavyPunch", "Punch" };

		auto tryCandidates = [this](const auto& candidates)
		{
			for (const std::string_view attackId : candidates)
			{
				if (!StartAttack(attackId)) continue;
				lastSelectedAttackId_ = std::string(attackId);
				return true;
			}
			return false;
		};

		if (bossPhase >= 3) return tryCandidates(phase3);
		if (bossPhase == 2) return tryCandidates(phase2);
		return tryCandidates(phase1);
	}

	void BossAttackComponent::RegisterDefaultAttacks()
	{
		if (!FindAttackData("Punch"))
		{
			AttackData data{ "Punch", "Melee", "Attack.Melee", 24.0f, 1.0f, 0.22f, 0.12f, 0.35f, 0.0f, 3.2f, 0.0f };
			data.maxHeightDifference = 3.0f;
			RegisterAttack(std::move(data), CreateAttackBehavior("Melee"));
		}
		if (!FindAttackData("HeavyPunch"))
		{
			AttackData data{ "HeavyPunch", "Melee", "Attack.Melee", 42.0f, 1.8f, 0.45f, 0.16f, 0.55f, 0.0f, 4.0f, 0.0f };
			data.maxHeightDifference = 3.5f;
			RegisterAttack(std::move(data), CreateAttackBehavior("Melee"));
		}
		if (!FindAttackData("Charge"))
		{
			AttackData data{ "Charge", "Charge", "Attack.Charge", 36.0f, 3.0f, 0.35f, 0.70f, 0.45f, 2.5f, 12.0f, 8.0f };
			data.maxHeightDifference = 2.5f;
			RegisterAttack(std::move(data), CreateAttackBehavior("Charge"));
		}
		if (!FindAttackData("Shockwave"))
		{
			AttackData data{ "Shockwave", "Shockwave", "Attack.Shockwave", 55.0f, 4.0f, 0.65f, 0.12f, 0.65f, 1.5f, 8.0f, 0.0f };
			data.maxHeightDifference = 2.0f; // 地面付近を伝わる衝撃波なので、高所Targetには命中させない。
			RegisterAttack(std::move(data), CreateAttackBehavior("Shockwave"));
		}
	}
} // namespace Ken4lowEngine
