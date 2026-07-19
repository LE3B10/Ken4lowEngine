#pragma once

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterHealthComponent;

	/// BossのHP容量、フェーズ判定、移行中の無敵時間を管理するComponent。
	class BossPhaseComponent final : public ActorComponent
	{
	public:
		/// 復元されたHP容量、閾値、現在フェーズ、無敵時間を安全な範囲へ補正する。
		void Initialize() override;

		/// 共通Healthを監視し、閾値を越えた場合だけフェーズを進める。
		void Update(float deltaTime) override;

		/// Component破棄時に、このComponentが開始した無敵状態だけを解除する。
		void Finalize() override;

		/// 現在HP容量、フェーズ、閾値、移行無敵時間をDebug表示する。
		void DrawImGui() override;

		std::string GetClassTypeName() const override { return "BossPhaseComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		int GetCurrentPhase() const { return currentPhase_; }
		unsigned int GetPhaseRevision() const { return phaseRevision_; }
		float GetConfiguredMaxHealth() const { return configuredMaxHealth_; }
		bool IsPhaseInvulnerabilityActive() const { return phaseInvulnerabilityActive_; }
		float GetPhaseInvulnerabilityRemaining() const { return phaseInvulnerabilityRemaining_; }

		/// Debug再検証用にPhase 1へ戻す。
		void ResetPhase();

	private:
		int EvaluatePhase(float healthRatio) const;
		void SanitizeSettings();
		void ApplyConfiguredHealthCapacity(CharacterHealthComponent& health);
		void BeginPhaseInvulnerability(CharacterHealthComponent& health);
		void UpdatePhaseInvulnerability(CharacterHealthComponent& health, float deltaTime);
		void EndPhaseInvulnerability(CharacterHealthComponent& health);

	private:
		float configuredMaxHealth_ = 2400.0f;
		float phase2HealthRatio_ = 0.70f;
		float phase3HealthRatio_ = 0.35f;
		float phaseInvulnerabilityDuration_ = 1.20f;
		float phaseInvulnerabilityRemaining_ = 0.0f;
		int currentPhase_ = 1;
		unsigned int phaseRevision_ = 0;
		bool healthCapacityApplied_ = false;
		bool phaseInvulnerabilityActive_ = false;
		bool wasInvulnerableBeforePhase_ = false;
	};
} // namespace Ken4lowEngine
