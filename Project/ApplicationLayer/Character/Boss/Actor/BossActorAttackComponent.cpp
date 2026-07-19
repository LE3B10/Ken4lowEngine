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
		ImGui::Text("Bossプロファイル: %s", attackProfile_.c_str());
		ImGui::Text("Boss選択: %s", lastSelectedAttackId_.c_str());
#endif
	}

	void BossAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		AttackComponent::ToJson(outJson);
		outJson["AttackProfile"] = attackProfile_;
	}

	void BossAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		AttackComponent::FromJson(inJson);
		attackProfile_ = inJson.value("AttackProfile", attackProfile_); // Prefabごとの攻撃構成をInitialize前に確定する。
		if (attackProfile_.empty()) attackProfile_ = "Guardian";
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

		if (attackProfile_ == "MineCrusher")
		{
			if (bossPhase >= 3)
			{
				if (distanceToTarget > 6.0f)
				{
					static constexpr std::array<std::string_view, 4> farCandidates{
						"FrenzyCharge", "FastShockwave", "Shockwave", "Charge"
					};
					return tryCandidates(farCandidates);
				}
				if (distanceToTarget > 3.0f)
				{
					static constexpr std::array<std::string_view, 5> middleCandidates{
						"FastShockwave", "GroundSlam", "Shockwave", "HeavyPunch", "FrenzyCharge"
					};
					return tryCandidates(middleCandidates);
				}
				static constexpr std::array<std::string_view, 4> nearCandidates{
					"GroundSlam", "RapidPunch", "HeavyPunch", "Punch"
				};
				return tryCandidates(nearCandidates);
			}

			if (bossPhase == 2)
			{
				if (distanceToTarget > 6.0f)
				{
					static constexpr std::array<std::string_view, 3> farCandidates{
						"Shockwave", "Charge", "GroundSlam"
					};
					return tryCandidates(farCandidates);
				}
				if (distanceToTarget > 3.0f)
				{
					static constexpr std::array<std::string_view, 5> middleCandidates{
						"GroundSlam", "Shockwave", "HeavyPunch", "Charge", "Punch"
					};
					return tryCandidates(middleCandidates);
				}
				static constexpr std::array<std::string_view, 3> nearCandidates{
					"GroundSlam", "HeavyPunch", "Punch"
				};
				return tryCandidates(nearCandidates);
			}

			if (distanceToTarget > 5.0f)
			{
				static constexpr std::array<std::string_view, 2> farCandidates{ "Charge", "Shockwave" };
				return tryCandidates(farCandidates);
			}
			static constexpr std::array<std::string_view, 3> nearCandidates{ "HeavyPunch", "Punch", "GroundSlam" };
			return tryCandidates(nearCandidates);
		}

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
		return tryCandidates(nearCandidates);
	}

	void BossAttackComponent::RegisterDefaultAttacks()
	{
		if (attackProfile_ == "MineCrusher")
		{
			RegisterMineCrusherAttacks();
			return;
		}
		RegisterGuardianAttacks();
	}

	void BossAttackComponent::RegisterGuardianAttacks()
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
			RegisterAttack(std::move(data), CreateAttackBehavior(behaviorType));
		};

		AttackData punch{ "Punch", "Melee", "Attack.Melee", 24.0f, 0.95f, 0.17f, 0.10f, 0.24f, 0.0f, 3.2f, 0.0f };
		punch.maxHeightDifference = 3.0f;
		upsertAttack(std::move(punch));

		AttackData heavyPunch{ "HeavyPunch", "Melee", "Attack.Melee", 42.0f, 1.65f, 0.32f, 0.13f, 0.36f, 0.0f, 4.0f, 0.0f };
		heavyPunch.maxHeightDifference = 3.5f;
		upsertAttack(std::move(heavyPunch));

		AttackData charge{ "Charge", "Charge", "Attack.Charge", 36.0f, 2.35f, 0.28f, 0.82f, 0.24f, 3.2f, 18.0f, 18.0f };
		charge.maxHeightDifference = 2.5f;
		upsertAttack(std::move(charge));

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

	void BossAttackComponent::RegisterMineCrusherAttacks()
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
			RegisterAttack(std::move(data), CreateAttackBehavior(behaviorType));
		};

		AttackData crusherClaw{ "Punch", "Melee", "Attack.Melee", 32.0f, 1.10f, 0.24f, 0.14f, 0.30f, 0.0f, 4.2f, 0.0f };
		crusherClaw.maxHeightDifference = 4.0f;
		upsertAttack(std::move(crusherClaw));

		AttackData drillHammer{ "HeavyPunch", "Melee", "Attack.Melee", 48.0f, 1.85f, 0.44f, 0.16f, 0.42f, 0.0f, 5.0f, 0.0f };
		drillHammer.maxHeightDifference = 4.5f;
		upsertAttack(std::move(drillHammer));

		AttackData tunnelCharge{ "Charge", "Charge", "Attack.Charge", 44.0f, 2.70f, 0.50f, 1.00f, 0.35f, 4.0f, 20.0f, 14.0f };
		tunnelCharge.maxHeightDifference = 4.0f;
		upsertAttack(std::move(tunnelCharge));

		AttackData oreBurst{ "Shockwave", "Shockwave", "Attack.Shockwave", 38.0f, 2.40f, 0.70f, 0.12f, 0.42f, 2.5f, 12.0f, 0.0f };
		oreBurst.maxHeightDifference = 5.0f;
		upsertAttack(std::move(oreBurst));

		AttackData caveIn{ "GroundSlam", "Shockwave", "Attack.Shockwave", 56.0f, 4.20f, 0.95f, 0.16f, 0.55f, 0.0f, 8.0f, 0.0f };
		caveIn.maxHeightDifference = 5.0f;
		upsertAttack(std::move(caveIn));

		AttackData crusherCombo{ "RapidPunch", "Melee", "Attack.Melee", 34.0f, 0.68f, 0.16f, 0.10f, 0.18f, 0.0f, 4.5f, 0.0f };
		crusherCombo.maxHeightDifference = 4.0f;
		upsertAttack(std::move(crusherCombo));

		AttackData tunnelRush{ "FrenzyCharge", "Charge", "Attack.Charge", 48.0f, 1.90f, 0.30f, 0.92f, 0.20f, 3.5f, 23.0f, 18.0f };
		tunnelRush.maxHeightDifference = 4.5f;
		upsertAttack(std::move(tunnelRush));

		AttackData rockBurst{ "FastShockwave", "Shockwave", "Attack.Shockwave", 42.0f, 1.55f, 0.38f, 0.10f, 0.24f, 1.5f, 12.5f, 0.0f };
		rockBurst.maxHeightDifference = 5.0f;
		upsertAttack(std::move(rockBurst)); // 坑道ボスは高低差を許容する範囲攻撃を多用し、立体Arenaでも待機状態になりにくくする。
	}
} // namespace Ken4lowEngine
