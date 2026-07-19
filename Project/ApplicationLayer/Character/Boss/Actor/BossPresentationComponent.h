#pragma once

#include <ActorComponent.h>

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	class BossAttackComponent;
	class CharacterActor;
	struct AttackEvent;

	/// フェーズ遷移、攻撃予兆、死亡の見せ方をBrainや攻撃ロジックから分離するComponent。
	class BossPresentationComponent final : public ActorComponent
	{
	public:
		/// 現在Phase Revisionを初期値として保持し、生成直後の不要な演出を防ぐ。
		void Initialize() override;

		/// Phase変更、攻撃予兆、Auraを更新してAnimationとVisualへ反映する。
		void Update(float deltaTime) override;

		/// 攻撃Listenerを解除し、古いAttack Componentへの参照を残さない。
		void Finalize() override;

		/// 演出状態、攻撃予兆、残り時間をDebug表示する。
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

		/// 死亡演出が指定時間まで進んだか返す。
		bool IsDeathPresentationComplete() const { return deathPresentationActive_ && elapsed_ >= deathPresentationDuration_; }

		/// 現在の演出状態名を返す。
		const std::string& GetStateName() const { return stateName_; }

		/// Debug再検証用に演出状態を初期化する。
		void ResetPresentation();

		/// Phase移行・範囲攻撃が要求したCamera Shakeを一度だけ取り出す。
		bool ConsumeCameraShakeRequest(float& outDuration, float& outAmplitude, float& outFrequency);

	private:
		/// JSON再読込後も現在のAttack ComponentへListenerを張り直す。
		void EnsureAttackListener();

		/// 攻撃イベントを予兆開始、着地演出、終了へ振り分ける。
		void HandleAttackEvent(const AttackEvent& event);

		/// Windup時間を使って攻撃固有の予兆を開始する。
		void StartAttackTelegraph(const AttackEvent& event);

		/// 予兆の明滅とパーティクル発生を進める。
		void UpdateAttackTelegraph(float deltaTime);

		/// 攻撃終了・割り込み時に予兆状態を解除する。
		void FinishAttackTelegraph();

		/// 攻撃IDに応じた予兆または着地パーティクルを発生させる。
		void EmitAttackTelegraphPulse(bool impact);

		/// Phase変更直後と継続Auraのパーティクルを発生させる。
		void EmitPhasePulse(bool initialBurst);

		/// Phase、予兆、死亡状態から全部位の色と発光を更新する。
		void ApplyVisualAppearance();

		/// Phase遷移中の攻撃を止め、専用Animationを開始する。
		void StartPhaseTransition(int phase);

		/// Controllerへ渡すCamera Shake要求を上書きせず強い値へ統合する。
		void RequestCameraShake(float duration, float amplitude, float frequency);

	private:
		BossAttackComponent* boundAttack_ = nullptr;
		CharacterActor* telegraphTarget_ = nullptr;
		std::uint64_t attackListenerId_ = 0;
		unsigned int observedPhaseRevision_ = 0;
		int presentedPhase_ = 1;
		float phaseTransitionDuration_ = 0.8f;
		float deathPresentationDuration_ = 1.2f;
		float telegraphParticleInterval_ = 0.10f;
		float phaseAuraInterval_ = 0.18f;
		float elapsed_ = 0.0f;
		float visualTime_ = 0.0f;
		float telegraphElapsed_ = 0.0f;
		float telegraphDuration_ = 0.0f;
		float telegraphParticleTimer_ = 0.0f;
		float phaseParticleTimer_ = 0.0f;
		float pendingShakeDuration_ = 0.0f;
		float pendingShakeAmplitude_ = 0.0f;
		float pendingShakeFrequency_ = 0.0f;
		bool phaseTransitionActive_ = false;
		bool deathPresentationActive_ = false;
		bool attackTelegraphActive_ = false;
		bool cameraShakePending_ = false;
		std::string activeAttackId_;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine