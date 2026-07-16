#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../Core/EnemyBase.h"
#include "../Core/EnemyType.h"

#include <AABB.h>

#include <vector>

namespace Ken4lowEngine
{
	class EnemyAIComponent;
	class EnemyAttackComponent;
	class EnemyEffectComponent;
	class MidRangeEnemyAIComponent;
	class MidRangeEnemyAttackComponent;
	class HumanoidVisualComponent;
	class WorldGaugeComponent;

	/// EnemyBase互換を維持しながら、近接・中距離敵のAIと攻撃をアーキタイプ別Componentへ委譲する本番Actor。
	class EnemyActor final : public ::EnemyBase
	{
	public:
		explicit EnemyActor(::EnemyType enemyType = ::EnemyType::Melee) : enemyType_(enemyType) {}

		/// 必要なComponentを不足分だけ生成し、アーキタイプ別の基礎値を設定する。
		void Initialize() override;

		/// EnemyBaseの地形・死亡処理とComponent更新を進め、表示用状態を同期する。
		void Update(float deltaTime) override;

		/// EnemyActorは通常更新内でRoot・Collider・Visualを確定するため、物理後の人型再同期を行わない。
		void PostPhysicsUpdate(float deltaTime) override
		{
			(void)deltaTime; // Bodyのワールド座標をVisual Rootのローカル座標として二重適用しない。
		}

		/// Difficulty Directorの倍率を現在アーキタイプのAIと攻撃Componentへ適用する。
		void ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyActor"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		::EnemyType GetEnemyType() const { return enemyType_; }

		/// IntroなどPlayerだけを進める区間でAI・攻撃・死亡時間の更新を一時停止する。
		void SetSimulationEnabled(bool enabled) { simulationEnabled_ = enabled; }
		bool IsSimulationEnabled() const { return simulationEnabled_; }

		/// 各AIと攻撃Componentへ同じ追跡対象を設定する。Initialize前の指定も保持する。
		void SetTargetActor(CharacterActor* targetActor);

		/// A* ComponentへStage障害物を設定する。Initialize前の指定も保持し、所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// Debug比較用ダメージをEnemyBaseの本番Damage経路へ適用する。
		CharacterDamageResult ApplyComparisonDamage(float amount);

		/// DebugSceneで同じ個体を再比較できるよう生存・位置・各専用状態を戻す。
		void ResetForComparison(const Vector3& worldPosition);

		/// 照準中だけ頭上HP Gaugeを表示する。
		void SetHealthBarVisible(bool visible);

		/// 自爆Componentだけが無敵判定を迂回して共通死亡演出へ接続する。
		void KillAfterSuicide();

		EnemyAIComponent* GetEnemyAIComponent();
		const EnemyAIComponent* GetEnemyAIComponent() const;
		EnemyAttackComponent* GetEnemyAttackComponent();
		const EnemyAttackComponent* GetEnemyAttackComponent() const;
		MidRangeEnemyAIComponent* GetMidRangeEnemyAIComponent();
		const MidRangeEnemyAIComponent* GetMidRangeEnemyAIComponent() const;
		MidRangeEnemyAttackComponent* GetMidRangeEnemyAttackComponent();
		const MidRangeEnemyAttackComponent* GetMidRangeEnemyAttackComponent() const;
		EnemyEffectComponent* GetEnemyEffectComponent();
		const EnemyEffectComponent* GetEnemyEffectComponent() const;
		HumanoidVisualComponent* GetHumanoidVisualComponent();
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const;
		WorldGaugeComponent* GetHealthGaugeComponent();
		const WorldGaugeComponent* GetHealthGaugeComponent() const;

		void TakeDamage(int amount) override;
		void TakeDamage(int amount, const Vector3& hitDir, float hitPower) override;

	protected:
		/// 共通Healthが死亡へ遷移した時点で全AI・攻撃・移動を停止する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		/// CharacterHealthの現在値をWorldGaugeへ反映する。
		void SyncHealthGauge();
		void ApplyPendingRuntimeBindings();
		void EnsureRuntimeStateInitialized();
		void EnsureArchetypeComponents();
		int GetConfiguredArchetypeMaxHp() const;

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
		::EnemyType enemyType_ = ::EnemyType::Melee;
		bool runtimeStateInitialized_ = false;
		bool simulationEnabled_ = true;
	};
} // namespace Ken4lowEngine
