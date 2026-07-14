#pragma once

#include <AABB.h>
#include <Scene/Actor/Character/CharacterActor.h>

#include <vector>

namespace Ken4lowEngine
{
	class EnemyAIComponent;
	class EnemyAttackComponent;
	class EnemyEffectComponent;
	class HumanoidVisualComponent;
	class WorldGaugeComponent;

	/// 通常敵の共通Character機能と敵専用AI・攻撃・Effect・頭上HP表示を束ねるActor。
	class EnemyActor final : public CharacterActor
	{
	public:
		/// 必要なComponentを不足分だけ生成し、旧MeleeEnemyと同じ基礎値を設定する。
		void Initialize() override;

		/// 共通更新後にCharacterHealthを頭上WorldGaugeへ同期する。
		void Update(float deltaTime) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyActor"; }

		/// AIと攻撃Componentの両方へ同じ追跡対象を設定する。
		void SetTargetActor(CharacterActor* targetActor);

		/// A* ComponentへStage障害物を設定する。所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// 比較用ダメージを共通HPへ適用し、受理時だけHit Effectを発生させる。
		CharacterDamageResult ApplyComparisonDamage(float amount);

		/// DebugSceneで同じ個体を再比較できるよう生存・位置・各専用状態を戻す。
		void ResetForComparison(const Vector3& worldPosition);

		/// 照準中だけ頭上HP Gaugeを表示する。
		void SetHealthBarVisible(bool visible);

		/// 通常敵の追跡判断を担当するComponentを返す。
		EnemyAIComponent* GetEnemyAIComponent();

		/// 通常敵の攻撃を担当するComponentを返す。
		EnemyAttackComponent* GetEnemyAttackComponent();

		/// 通常敵のEffectを担当するComponentを返す。
		EnemyEffectComponent* GetEnemyEffectComponent();

		/// 通常敵の全部位描画を担当するComponentを返す。
		HumanoidVisualComponent* GetHumanoidVisualComponent();

		/// 通常敵の頭上HP表示を担当するComponentを返す。
		WorldGaugeComponent* GetHealthGaugeComponent();

	protected:
		/// 死亡時はAI・攻撃・移動・Colliderを止め、死亡Effectだけを開始する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		/// CharacterHealthの現在値をWorldGaugeへ反映する。
		void SyncHealthGauge();
	};
} // namespace Ken4lowEngine
