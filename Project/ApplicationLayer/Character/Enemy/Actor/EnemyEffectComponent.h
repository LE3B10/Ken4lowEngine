#pragma once

#include "ApplicationLayer/Character/Enemy/Effects/EnemyParticleEffectSystem.h"

#include <ActorComponent.h>
#include <Vector3.h>

#include <string>

namespace Ken4lowEngine
{
	/// 通常敵の被弾・死亡Effectと死亡後の表示終了を、EnemyActor本体から分離するComponent。
	class EnemyEffectComponent final : public ActorComponent
	{
	public:
		/// 共有Enemy Effect Systemを一度だけ初期化する。
		void Initialize() override;

		/// 死亡演出時間を進め、終了時に人型表示をまとめて隠す。
		void Update(float deltaTime) override;

		/// Effect発生回数と死亡演出状態をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyEffectComponent"; }

		/// 死亡表示時間をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから死亡表示時間を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 被弾位置へ旧通常敵と同じHit Effectを発生させる。
		void TriggerHitEffect(const Vector3& worldPosition);

		/// 死亡位置へ旧通常敵と同じDeath Effectを発生させ、表示終了タイマーを開始する。
		void TriggerDeathEffect(const Vector3& worldPosition);

		/// 比較再実行用に表示、色、Effect計測値を初期状態へ戻す。
		void ResetEffectState();

		/// 死亡Effectの発生回数を返す。
		int GetDeathEffectCount() const { return deathEffectCount_; }

		/// 被弾Effectの発生回数を返す。
		int GetHitEffectCount() const { return hitEffectCount_; }

		/// 死亡演出が進行中か返す。
		bool IsDeathEffectActive() const { return deathEffectActive_; }

	private:
		EnemyParticleEffectSystem effectSystem_{};
		float deathPresentationDuration_ = 1.8f;
		float deathTimer_ = 0.0f;
		int hitEffectCount_ = 0;
		int deathEffectCount_ = 0;
		bool deathEffectActive_ = false;
	};
} // namespace Ken4lowEngine
