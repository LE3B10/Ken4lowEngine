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

		if (bossPhase >= 3)
		{
			if (distanceToTarget > 4.5f)
			{
				static constexpr std::array<std::string_view, 4> farCandidates{
					"FrenzyCharge", "FastShockwave", "Charge", "Shockwave"
				};
				return tryCandidates(farCandidates);
			}
			if (distanceToTarget > 2.2f)
			{
				static constexpr std::array<std::string_view, 6> middleCandidates{
					"FastShockwave", "RapidPunch", "GroundSlam", "FrenzyCharge", "HeavyPunch", "Punch"
				};
				return tryCandidates(middleCandidates);
			}

			static constexpr std::array<std::string_view, 4> nearCandidates{
				"RapidPunch", "GroundSlam", "Punch", "HeavyPunch"
			};
			return tryCandidates(nearCandidates);
		}

		if (bossPhase == 2)
		{
			if (distanceToTarget > 4.5f)
			{
				static constexpr std::array<std::string_view, 3> farCandidates{
					"Charge", "Shockwave", "GroundSlam"
				};
				return tryCandidates(farCandidates);
			}
			if (distanceToTarget > 2.2f)
			{
				static constexpr std::array<std::string_view, 5> middleCandidates{
					"Shockwave", "GroundSlam", "HeavyPunch", "Punch", "Charge"
				};
				return tryCandidates(middleCandidates);
			}

			static constexpr std::array<std::string_view, 3> nearCandidates{
				"GroundSlam", "HeavyPunch", "Punch"
			};
			return tryCandidates(nearCandidates);
		}

		if (distanceToTarget > 3.4f)
		{
			static constexpr std::array<std::string_view, 1> farCandidates{ "Charge" };
			return tryCandidates(farCandidates);
		}

		static constexpr std::array<std::string_view, 2> nearCandidates{ "HeavyPunch", "Punch" };
		return tryCandidates(nearCandidates); // Phase 1は突進で接近し、近距離では既存の近接攻撃へ切り替える。
	}

	void BossAttackComponent::RegisterDefaultAttacks()
	{
		auto upsertAttack = [this](AttackData data)
		{
			const std::string attackId = data.id;
			if (FindAttackData(attackId))
			{
				ConfigureAttack(attackId, data);
				return;
			}
			const std::string behaviorType = data.behaviorType;
			RegisterAttack(std::move(data), CreateAttackBehavior(behaviorType)); // move後のAttackDataを参照せずBehaviorを生成する。
		};

		// 攻撃判定と見た目の尺を同時に短縮し、予兆と命中タイミングをずらさない。
		AttackData punch{ "Punch", "Melee", "Attack.Melee", 24.0f, 0.95f, 0.17f, 0.10f, 0.24f, 0.0f, 3.2f, 0.0f };
		punch.maxHeightDifference = 3.0f;
		upsertAttack(std::move(punch));

		AttackData heavyPunch{ "HeavyPunch", "Melee", "Attack.Melee", 42.0f, 1.65f, 0.32f, 0.13f, 0.36f, 0.0f, 4.0f, 0.0f };
		heavyPunch.maxHeightDifference = 3.5f;
		upsertAttack(std::move(heavyPunch));

		AttackData charge{ "Charge", "Charge", "Attack.Charge", 36.0f, 2.35f, 0.28f, 0.82f, 0.24f, 3.2f, 18.0f, 18.0f };
		charge.maxHeightDifference = 2.5f;
		upsertAttack(std::move(charge)); // 旧Guardian相当の速度へ戻し、最長距離から近接圏まで一気に詰める。

		AttackData shockwave{ "Shockwave", "Shockwave", "Attack.Shockwave", 44.0f, 3.15f, 0.54f, 0.10f, 0.38f, 2.0f, 10.0f, 0.0f };
		shockwave.maxHeightDifference = 2.0f;
		upsertAttack(std::move(shockwave));

		AttackData groundSlam{ "GroundSlam", "Shockwave", "Attack.Shockwave", 52.0f, 3.75f, 0.62f, 0.11f, 0.45f, 0.0f, 5.8f, 0.0f };
		groundSlam.maxHeightDifference = 2.3f;
		upsertAttack(std::move(groundSlam));

		AttackData rapidPunch{ "RapidPunch", "Melee", "Attack.Melee", 30.0f, 0.52f, 0.11f, 0.08f, 0.14f, 0.0f, 3.4f, 0.0f };
		rapidPunch.maxHeightDifference = 3.0f;
		upsertAttack(std::move(rapidPunch));

		AttackData frenzyCharge{ "FrenzyCharge", "Charge", "Attack.Charge", 42.0f, 1.55f, 0.16f, 0.78f, 0.13f, 3.0f, 22.0f, 24.0f };
		frenzyCharge.maxHeightDifference = 2.5f;
		upsertAttack(std::move(frenzyCharge));

		AttackData fastShockwave{ "FastShockwave", "Shockwave", "Attack.Shockwave", 48.0f, 1.90f, 0.29f, 0.08f, 0.21f, 1.5f, 10.5f, 0.0f };
		fastShockwave.maxHeightDifference = 2.0f;
		upsertAttack(std::move(fastShockwave));
	}
} // namespace Ken4lowEngine