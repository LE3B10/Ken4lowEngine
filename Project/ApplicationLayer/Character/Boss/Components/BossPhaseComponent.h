#pragma once

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterHealthComponent;

	/// CharacterHealthComponentのHP割合からBossフェーズと移行中の無敵時間を管理するComponent。
	class BossPhaseComponent final : public ActorComponent
	{
	public:
		/// 復元された閾値、現在フェーズ、無敵時間を安全な範囲へ補正する。
		void Initialize() override;

		/// 共通Healthを監視し、閾値を越えた場合だけフェーズを進める。
		void Update(float deltaTime) override;

		/// Component破棄時に、このComponentが開始した無敵状態だけを解除する。
		void Finalize() override;

		/// 現在フェーズ、閾値、移行無敵時間をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossPhaseComponent"; }

		/// フェーズ閾値と移行無敵時間をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからフェーズ設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 現在フェーズを1から3の範囲で返す。
		int GetCurrentPhase() const { return currentPhase_; }

		/// フェーズ変更を検知するための単調増加Revisionを返す。
		unsigned int GetPhaseRevision() const { return phaseRevision_; }

		/// フェーズ移行による無敵時間が進行中か返す。
		bool IsPhaseInvulnerabilityActive() const { return phaseInvulnerabilityActive_; }

		/// 現在残っているフェーズ移行無敵時間を返す。
		float GetPhaseInvulnerabilityRemaining() const { return phaseInvulnerabilityRemaining_; }

		/// Debug再検証用にPhase 1へ戻す。
		void ResetPhase();

	private:
		/// HP割合から到達済みフェーズを決定する。
		int EvaluatePhase(float healthRatio) const;

		/// 外部入力された閾値と無敵時間を安全な範囲へ補正する。
		void SanitizeSettings();

		/// フェーズ移行時の無敵時間を開始する。
		void BeginPhaseInvulnerability(CharacterHealthComponent& health);

		/// 経過時間を進め、終了時に以前の無敵状態へ戻す。
		void UpdatePhaseInvulnerability(CharacterHealthComponent& health, float deltaTime);

		/// このComponentが開始した無敵状態を終了する。
		void EndPhaseInvulnerability(CharacterHealthComponent& health);

	private:
		float phase2HealthRatio_ = 0.70f;
		float phase3HealthRatio_ = 0.35f;
		float phaseInvulnerabilityDuration_ = 1.20f;
		float phaseInvulnerabilityRemaining_ = 0.0f;
		int currentPhase_ = 1;
		unsigned int phaseRevision_ = 0;
		bool phaseInvulnerabilityActive_ = false;
		bool wasInvulnerableBeforePhase_ = false;
	};
} // namespace Ken4lowEngine
