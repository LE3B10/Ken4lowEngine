#pragma once

#include "../Core/EnemyBase.h"

#include <AABB.h>

#include <vector>

namespace Ken4lowEngine
{
	class EnemyAIComponent;
	class EnemyAttackComponent;
	class EnemyEffectComponent;
	class HumanoidVisualComponent;
	class WorldGaugeComponent;

	/// EnemyBase互換を維持しながら、近接敵のAI・攻撃・表示同期を専用Componentへ委譲する本番Actor。
	class EnemyActor final : public ::EnemyBase
	{
	public:
		/// 必要なComponentを不足分だけ生成し、旧MeleeEnemyと同じ基礎値を設定する。
		void Initialize() override;

		/// EnemyBaseの地形・死亡処理とComponent更新を進め、表示用状態を同期する。
		void Update(float deltaTime) override;

		/// Difficulty Directorの倍率をAIと攻撃Componentへ適用する。
		void ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyActor"; }

		/// AIと攻撃Componentの両方へ同じ追跡対象を設定する。Initialize前の指定も保持する。
		void SetTargetActor(CharacterActor* targetActor);

		/// A* ComponentへStage障害物を設定する。Initialize前の指定も保持し、所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// Debug比較用ダメージをEnemyBaseの本番Damage経路へ適用する。
		CharacterDamageResult ApplyComparisonDamage(float amount);

		/// DebugSceneで同じ個体を再比較できるよう生存・位置・各専用状態を戻す。
		void ResetForComparison(const Vector3& worldPosition);

		/// 照準中だけ頭上HP Gaugeを表示する。
		void SetHealthBarVisible(bool visible);

		EnemyAIComponent* GetEnemyAIComponent();
		const EnemyAIComponent* GetEnemyAIComponent() const;
		EnemyAttackComponent* GetEnemyAttackComponent();
		const EnemyAttackComponent* GetEnemyAttackComponent() const;
		EnemyEffectComponent* GetEnemyEffectComponent();
		const EnemyEffectComponent* GetEnemyEffectComponent() const;
		HumanoidVisualComponent* GetHumanoidVisualComponent();
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const;
		WorldGaugeComponent* GetHealthGaugeComponent();
		const WorldGaugeComponent* GetHealthGaugeComponent() const;

	protected:
		/// 共通Healthが死亡へ遷移した時点でAI・攻撃・移動を停止する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		/// CharacterHealthの現在値をWorldGaugeへ反映する。
		void SyncHealthGauge();
		void ApplyPendingRuntimeBindings();

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
	};
} // namespace Ken4lowEngine
