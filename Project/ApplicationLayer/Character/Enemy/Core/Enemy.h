#pragma once
#include "Vector3.h"
#include "EnemyBase.h"
#include "IEnemyState.h"

#include <memory>

/// ----------前方宣言 ---------- ///
class BulletManager;
class CollisionManager;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///							Enemy
/// -------------------------------------------------------------
class Enemy final : public EnemyBase
{
private: /// ---------- 列挙型 ---------- ///

	enum class AnimState
	{
		Idle,
		Move,
		Shoot,
		Search,
		Dead,
	};

private: /// ---------- 構造体 ---------- ///

	// 視覚・感知の設定
	struct EnemyPerceptionConfig
	{
		float viewRange = 30.0f;		   // 視認可能距離
		float viewFovDeg = 120.0f;         // 視野角（左右合計）
		float viewFovVerticalDeg = 85.0f;  // 縦（上下合計）
		float eyeHeight = 1.2f;            // 目の高さ
		float targetEyeHeight = 1.2f;      // ターゲット側の高さ
		bool useLOS = true;                // 遮蔽チェックをするか
		bool useVerticalFov = false;       // 縦FOVを使うか
		float nearDetectRadius = 6.0f;     // この距離以内は横FOVを無視して気付きやすくする
		float nearLoseRadius = 7.5f;       // 近距離で見失いにくくするヒステリシス
		float loseSightGraceSec = 1.2f;    // ターゲットを見失ってから完全に見失うまでの猶予時間
	};

	// 移動 / 戦闘の設定
	struct EnemyCombatConfig
	{
		float attackRange = 15.0f;   // 攻撃可能距離（射程）
		float idealRangeMin = 8.0f;  // ここより近いと離脱寄り
		float idealRangeMax = 13.0f; // ここより遠いと接近寄り
		float fireInterval = 0.35f;  // 攻撃間隔（秒）
		float bulletSpeed = 18.0f;   // 弾の速度
		float bulletLifeSec = 3.0f;  // 弾の寿命（秒）
		int   bulletDamage = 50;     // 弾のダメージ
		float muzzleHeight = 1.2f;   // マズルの高さ
		float searchDuration = 4.0f; // 索敵状態の滞在時間
	};

	// 移動設定
	struct EnemyMovementConfig
	{
		float approachSpeed = 3.2f;
		float retreatSpeed = 3.0f;
		float strafeSpeed = 2.8f;
		float searchMoveSpeed = 2.3f;
		float strafeSwitchMinSec = 0.4f;
		float strafeSwitchMaxSec = 1.0f;
	};

	// 敵の記憶
	struct EnemyMemory
	{
		K4E::Vector3 lastSeenPos{ 0.0f, 0.0f, 0.0f }; // 最後に見たターゲットの位置
		float timeSinceSeen = 9999.0f;				  // 最後に見てからの経過時間（秒）
	};

	// 敵の向き（Yawのみ）
	struct EnemyFacing
	{
		float yawRad = 0.0f; // Yaw角（ラジアン）
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	void Update(float deltaTime) override;

public: /// ---------- 外部からのアクセス ---------- ///

	// 依存の注入
	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetBulletManager(BulletManager* bm) { bulletManager_ = bm; }
	void SetCollisionManager(CollisionManager* cm) { collisionManager_ = cm; }

public: /// ---------- 状態管理 ---------- ///

	void ChangeState(std::unique_ptr<IEnemyState> nextState);
	void ChangeStateToIdle();
	void ChangeStateToCombatMove();
	void ChangeStateToShoot();
	void ChangeStateToSearch();
	void ChangeStateToDead();

public: /// ---------- アクセサ ---------- ///

	bool HasTarget() const { return target_ != nullptr; }
	K4E::Vector3 GetTargetPosition() const;
	float GetDistanceToTarget() const;

	void StopMove();
	void MoveTowards(const K4E::Vector3& targetPos);
	void MoveTowards(const K4E::Vector3& targetPos, float speed);
	void MoveAwayFrom(const K4E::Vector3& targetPos, float speed);
	void MoveStrafeAround(const K4E::Vector3& targetPos, float sign, float speed);
	void MoveToLastSeen(float speed);
	void FaceTo(const K4E::Vector3& targetPos);
	void FireAt(const K4E::Vector3& targetPos);

	void RememberLastSeenTarget(const K4E::Vector3& targetPos)
	{
		memory_.lastSeenPos = targetPos;
		memory_.timeSinceSeen = 0.0f;
	}

	const K4E::Vector3& GetLastSeenTargetPosition() const { return memory_.lastSeenPos; }
	float GetTimeSinceSeen() const { return memory_.timeSinceSeen; }
	bool HasLostTarget() const { return memory_.timeSinceSeen > perception_.loseSightGraceSec; }

	bool CanSeeTargetPublic(const K4E::Vector3& targetPos, float distToTarget) { return CanSeeTarget(targetPos, distToTarget); }

	bool CanShootTargetPublic(const K4E::Vector3& targetPos) const { return CanShootTarget(targetPos); }

	float GetAttackRange() const { return combat_.attackRange; }

	float GetIdealRangeMin() const { return combat_.idealRangeMin; }
	float GetIdealRangeMax() const { return combat_.idealRangeMax; }
	float GetSearchDuration() const { return combat_.searchDuration; }

	float GetApproachSpeed() const { return movement_.approachSpeed; }
	float GetRetreatSpeed() const { return movement_.retreatSpeed; }
	float GetStrafeSpeed() const { return movement_.strafeSpeed; }
	float GetSearchMoveSpeed() const { return movement_.searchMoveSpeed; }
	float GetFireInterval() const { return combat_.fireInterval; }

	void UpdateStrafeDecision(float dt);
	float GetCurrentStrafeSign() const { return currentStrafeSign_; }

	void PlayIdleAnimation();
	void PlayMoveAnimation(float moveSpeed = -1.0f);
	void PlayShootAnimation();
	void PlaySearchAnimation(float moveSpeed = -1.0f);
	void PlayDeadAnimation();

protected: /// ---------- EnemyBaseからの通知 ---------- ///

	// EnemyBaseからの弾ヒット
	void OnBulletHit(K4E::Collider* bulletCollider) override;

private:

	bool HasLineOfSight(const K4E::Vector3& fromPos, const K4E::Vector3& toPos) const;

private: /// ---------- 視界判定 ---------- ///

	bool CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget);
	bool CanShootTarget(const K4E::Vector3& targetPos) const;

private: /// ---------- 内部処理 ---------- ///

	void MoveInDirectionXZ(const K4E::Vector3& dir, float speed);
	void SetAnimState(AnimState next);
	void UpdateAnimation(float dt);

private: /// ---------- メンバ変数 ---------- ///

	// 外部依存
	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	CollisionManager* collisionManager_ = nullptr;

	EnemyPerceptionConfig perception_{};

	EnemyCombatConfig combat_{};

	EnemyMovementConfig movement_{};

	EnemyMemory memory_{};

	EnemyFacing facing_{};

	std::unique_ptr<IEnemyState> state_ = nullptr;
	float fireCooldown_ = 0.0f;
	float strafeDecisionTimer_ = 0.0f;
	float currentStrafeSign_ = 1.0f;

	AnimState animState_ = AnimState::Idle;
	float animTime_ = 0.0f;
	float animMoveRate_ = 1.0f;
};
