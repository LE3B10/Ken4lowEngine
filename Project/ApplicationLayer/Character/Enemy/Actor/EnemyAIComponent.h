#pragma once

#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"

#include <ActorComponent.h>
#include <AABB.h>
#include <Vector3.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class CharacterActor;
	class SceneComponent;

	/// 通常敵の追跡判断とA*経路選択を、Actor本体や描画処理から分離して管理するComponent。
	class EnemyAIComponent final : public ActorComponent
	{
	public:
		/// 旧MeleeEnemyと同じ移動・停止・A*調整値でNavigatorを初期化する。
		void Initialize() override;

		/// TargetまでのA* Waypointを選び、移動速度だけをCharacterMovementComponentへ渡す。
		void Update(float deltaTime) override;

		/// AI調整値と現在の経路状態をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyAIComponent"; }

		/// AIの比較用調整値をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからAIの比較用調整値を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 追跡対象Actorを設定する。所有権は移さない。
		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }

		/// A*で回避対象にするStage障害物を設定する。所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// 旧敵との比較に使用する基本移動速度を返す。
		float GetMoveSpeed() const { return moveSpeed_; }

		/// 進行方向またはTarget方向へ向く最大Yaw回転速度を返す。
		float GetRotateSpeed() const { return rotateSpeed_; }

		/// 攻撃へ切り替える距離を返す。
		float GetAttackStartRange() const { return attackStartRange_; }

		/// 現在A*経路が有効か返す。
		bool HasPath() const { return pathFound_; }

		/// 現在のA*経路ノード数を返す。
		size_t GetPathNodeCount() const { return navigator_.GetCurrentPath().size(); }

		/// 現在のAI状態名を返す。
		const std::string& GetStateName() const { return stateName_; }

		/// 現在のTargetまでのXZ距離を返す。
		float GetDistanceToTarget() const { return distanceToTarget_; }

		/// 死亡演出中など、AIと移動出力を一括停止する。
		void StopBehavior();

		/// 比較再実行時に経路と状態を初期値へ戻す。
		void ResetBehavior();

	private:
		/// 旧Enemyと同じ前方規約でRootのYawを指定XZ方向へ滑らかに合わせる。
		void FaceDirection(SceneComponent& root, const Vector3& direction, float deltaTime);

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
		EnemyAStarNavigator navigator_{};
		float moveSpeed_ = 3.2f;
		float rotateSpeed_ = 8.0f;
		float stopDistance_ = 1.8f;
		float attackStartRange_ = 2.4f;
		float distanceToTarget_ = 0.0f;
		bool pathFound_ = false;
		bool behaviorEnabled_ = true;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine
