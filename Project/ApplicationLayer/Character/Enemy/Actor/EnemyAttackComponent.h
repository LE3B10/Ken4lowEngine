#pragma once

#include <Scene/Actor/Character/AttackComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterActor;
	class EnemyAIComponent;

	/// 旧通常敵APIを保ちながら攻撃本体を共通AttackComponentへ委譲する移行Adapter。
	class EnemyAttackComponent final : public AttackComponent
	{
	public:
		/// 攻撃タイマーを比較開始状態へ初期化する。
		void Initialize() override;

		/// AIが攻撃範囲内のときだけ、登録済みMelee攻撃の開始を共通基盤へ要求する。
		void Update(float deltaTime) override;

		/// 攻撃調整値と実測回数をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyAttackComponent"; }

		/// 攻撃間隔とダメージをActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから攻撃調整値を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// ダメージ適用先のCharacterActorを共通AttackComponentへ設定する。
		void SetTargetActor(CharacterActor* targetActor) { AttackComponent::SetTargetActor(targetActor); }

		/// 死亡中などに攻撃処理を停止する。
		void StopAttacking() { SetAttackEnabled(false); }

		/// 比較再実行用に攻撃タイマーと計測値をリセットする。
		using AttackComponent::ResetAttackState;

		/// 旧Scratch設定と比較する攻撃クールダウンを返す。
		float GetAttackCooldown() const;

		/// Scratchの予備・有効・復帰・Cooldownを含む命中間隔を返す。
		float GetExpectedHitInterval() const;

		/// 旧Scratch設定と比較する1回のダメージを返す。
		float GetAttackDamage() const;

	private:
		/// JSON復元前後のどちらでも通常敵用Melee攻撃が1件だけ存在するよう補完する。
		void EnsureDefaultMeleeAttack();
	};
} // namespace Ken4lowEngine
