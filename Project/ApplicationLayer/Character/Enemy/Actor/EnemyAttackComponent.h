#pragma once

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterActor;
	class EnemyAIComponent;

	/// 通常敵の攻撃間隔、ダメージ適用、命中回数をAI判断や表示から分離するComponent。
	class EnemyAttackComponent final : public ActorComponent
	{
	public:
		/// 攻撃タイマーを比較開始状態へ初期化する。
		void Initialize() override;

		/// AIが攻撃範囲内のときだけ、設定間隔ごとにTargetへダメージを適用する。
		void Update(float deltaTime) override;

		/// 攻撃調整値と実測回数をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyAttackComponent"; }

		/// 攻撃間隔とダメージをActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから攻撃調整値を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// ダメージ適用先のCharacterActorを設定する。所有権は移さない。
		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }

		/// 死亡中などに攻撃処理を停止する。
		void StopAttacking() { attackEnabled_ = false; }

		/// 比較再実行用に攻撃タイマーと計測値をリセットする。
		void ResetAttackState();

		/// 旧Scratch設定と比較する攻撃クールダウンを返す。
		float GetAttackCooldown() const { return attackCooldown_; }

		/// Scratchの予備・有効・復帰・Cooldownを含む命中間隔を返す。
		float GetExpectedHitInterval() const { return attackStartDelay_ + attackActiveTime_ + attackRecoveryTime_ + attackCooldown_; }

		/// 旧Scratch設定と比較する1回のダメージを返す。
		float GetAttackDamage() const { return attackDamage_; }

		/// 実際にTargetへ受理された攻撃回数を返す。
		int GetAcceptedHitCount() const { return acceptedHitCount_; }

		/// 最後に攻撃が受理された時刻からの経過時間を返す。
		float GetLastMeasuredInterval() const { return lastMeasuredInterval_; }

	private:
		CharacterActor* targetActor_ = nullptr;
		float attackCooldown_ = 0.55f;
		float attackDamage_ = 8.0f;
		float attackStartDelay_ = 0.12f;
		float attackActiveTime_ = 0.10f;
		float attackRecoveryTime_ = 0.35f;
		float cooldownRemaining_ = 0.0f;
		float elapsedSinceAcceptedHit_ = 0.0f;
		float lastMeasuredInterval_ = 0.0f;
		int acceptedHitCount_ = 0;
		bool attackEnabled_ = true;
	};
} // namespace Ken4lowEngine
