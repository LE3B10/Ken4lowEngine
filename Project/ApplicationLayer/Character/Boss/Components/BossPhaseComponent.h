#pragma once

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	/// CharacterHealthComponentのHP割合からBossフェーズだけを判定するComponent。
	class BossPhaseComponent final : public ActorComponent
	{
	public:
		/// 復元された閾値と現在フェーズを安全な範囲へ補正する。
		void Initialize() override;

		/// 共通Healthを監視し、閾値を越えた場合だけフェーズを進める。
		void Update(float deltaTime) override;

		/// 現在フェーズと閾値をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossPhaseComponent"; }

		/// フェーズ閾値と現在値をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからフェーズ設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 現在フェーズを1から3の範囲で返す。
		int GetCurrentPhase() const { return currentPhase_; }

		/// フェーズ変更を検知するための単調増加Revisionを返す。
		unsigned int GetPhaseRevision() const { return phaseRevision_; }

		/// Debug再検証用にPhase 1へ戻す。
		void ResetPhase();

	private:
		/// HP割合から到達済みフェーズを決定する。
		int EvaluatePhase(float healthRatio) const;

		/// 外部入力された閾値をPhase順へ補正する。
		void SanitizeThresholds();

	private:
		float phase2HealthRatio_ = 0.70f;
		float phase3HealthRatio_ = 0.35f;
		int currentPhase_ = 1;
		unsigned int phaseRevision_ = 0;
	};
} // namespace Ken4lowEngine
