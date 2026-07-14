#pragma once

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	/// フェーズ遷移と死亡の見せ方をBrainや攻撃ロジックから分離するComponent。
	class BossPresentationComponent final : public ActorComponent
	{
	public:
		/// 現在Phase Revisionを初期値として保持し、生成直後の不要な演出を防ぐ。
		void Initialize() override;

		/// Phase変更を検出し、Animationへ演出ポーズを要求する。
		void Update(float deltaTime) override;

		/// 演出状態と残り時間をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossPresentationComponent"; }

		/// 演出時間設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから演出時間設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 死亡時の攻撃中断と死亡ポーズを開始する。
		void StartDeathPresentation();

		/// Brainが行動を止めるべき演出中か返す。
		bool IsBlockingBehavior() const { return phaseTransitionActive_ || deathPresentationActive_; }

		/// 現在の演出状態名を返す。
		const std::string& GetStateName() const { return stateName_; }

		/// Debug再検証用に演出状態を初期化する。
		void ResetPresentation();

	private:
		/// Phase遷移中の攻撃を止め、専用Animationを開始する。
		void StartPhaseTransition(int phase);

	private:
		unsigned int observedPhaseRevision_ = 0;
		float phaseTransitionDuration_ = 0.8f;
		float deathPresentationDuration_ = 1.2f;
		float elapsed_ = 0.0f;
		bool phaseTransitionActive_ = false;
		bool deathPresentationActive_ = false;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine
