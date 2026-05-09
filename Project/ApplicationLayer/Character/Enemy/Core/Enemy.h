#pragma once
#include "Vector3.h"
#include "EnemyBase.h"
#include "IEnemyState.h"
#include <EnemyAStarNavigator.h>
#include <EnemyAimController.h>
#include <EnemyCoverController.h>
#include <EnemyCoverSelector.h>
#include <EnemyEvadeController.h>
#include <EnemyRetreatController.h>
#include <EnemyRetreatDecisionMemory.h>
#include <EnemyStuckController.h>
#include <EnemyTraitProfile.h>

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
		float attackRange = 26.0f;       // 戦闘を継続する最大距離の目安
		float fireRange = 23.0f;         // 射線が通るときに実際に撃つ距離
		float minCombatRange = 9.0f;     // これより近い場合は撃たずに距離を取る
		float idealCombatRange = 15.0f;  // 銃撃戦で維持したい中心距離
		float maxCombatRange = 23.0f;    // これより遠い場合は射撃可能距離まで前進する
		float retreatRange = 7.0f;       // 強制後退を開始する距離
		float idealRangeMin = 10.0f;     // 互換用: 通常時の適正距離(近側)
		float idealRangeMax = 17.5f;     // 互換用: 通常時の適正距離(遠側)
		float tooCloseRange = 7.0f;      // 互換用: 近すぎるので優先的に離脱
		float tooFarRange = 25.0f;       // 互換用: 遠すぎるので優先的に接近
		float fireInterval = 0.22f;      // バースト中の1発ごとの射撃間隔（秒）
		float enemyBulletSpeed = 48.0f;       // 弾の速度
		float accuracyConeRad = 0.026f;  // 距離・移動補正前の命中ブレ（ラジアン）
		float aimErrorAngleRad = 0.018f; // 固定の照準誤差（ラジアン）
		int   burstCount = 4;            // 1回の攻撃で撃つ最大弾数
		float burstInterval = 0.10f;     // バースト内の最短発射間隔（秒）
		float postBurstWait = 0.75f;     // バースト後の待機時間（秒）
		int   magazineSize = 12;         // マガジン弾数
		float reloadTime = 1.85f;        // リロード時間（秒）
		bool  reloadMoveEnabled = true;  // リロード中に移動するか
		float bulletLifeSec = 3.0f;      // 弾の寿命（秒）
		int   bulletDamage = 5;          // 弾のダメージ
		float muzzleHeight = 1.2f;   // マズルの高さ
		float searchDuration = 5.0f; // 索敵状態の滞在時間
		float losRepositionEvalSec = 0.35f; // 射線調整の再評価間隔
		float shootRepositionEvalSec = 0.14f; // 射撃中の短周期再評価
		float shootMaxStaySec = 1.05f; // 射撃状態で粘る最大時間
	};

	// 低HP時の生存行動の設定
	struct EnemySurvivalConfig
	{
		float lowHpThresholdRate = 0.35f;
		float retreatCooldown = 2.5f;
		float lowHpRetreatDistance = 20.0f;
		float lowHpReturnDistance = 28.0f;
		float lowHpRetreatSpeedScale = 1.45f;
		float lowHpShootRange = 15.0f;
		float lowHpShootStaySec = 0.35f;
		float retreatDecisionInterval = 0.18f;
	};

	struct EnemyReactionConfig
	{
		float hitReactionTime = 0.95f;
		float hitReactionMoveWeight = 0.8f;
		float hitChainWindow = 1.15f;
		float evadeWeight = 0.72f;
		float coverBias = 0.64f;
	};

	struct EnemyCoverConfig
	{
		float coverSearchRadius = 10.5f;
		int coverSampleCount = 16;
		float coverDistanceScoreWeight = 0.75f;
		float coverRepathInterval = 0.2f;
		float coverStayTime = 0.9f;
		float peekOffset = 1.55f;
		float peekExposeMinSec = 0.3f;
		float peekExposeMaxSec = 0.75f;
		float peekHideMinSec = 0.35f;
		float peekHideMaxSec = 0.95f;
	};

	// 移動設定
	struct EnemyMovementConfig
	{
		float approachSpeed = 3.2f;
		float retreatSpeed = 3.0f;
		float strafeSpeed = 2.8f;
		float searchMoveSpeed = 2.3f;
		float strafeSwitchMinSec = 0.4f;
		float strafeSwitchMaxSec = 1.25f;
		float losProbeDistance = 2.6f;
		float tacticalBlend = 0.45f;
		float shootMicroStrafeSpeed = 1.35f;
		float jumpProbeDistance = 1.05f;
		float jumpStepHeight = 1.05f;
		float jumpVelocity = 6.2f;
		float jumpCooldown = 0.72f;
	};

	// 通常時の徘徊設定
	struct EnemyWanderConfig
	{
		float roamRadius = 10.0f;
		float minTargetDistance = 2.0f;
		float reachDistance = 0.9f;
		float wanderMoveSpeed = 2.0f;
		float retargetIntervalMin = 1.8f;
		float retargetIntervalMax = 3.8f;
		float stuckCheckInterval = 0.8f;
		float stuckDistance = 0.25f;

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
	void DrawImGui() override;
	void TakeDamage(int amount) override;
	void TakeDamage(int amount, const K4E::Vector3& hitDir, float hitPower) override;

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
	const char* GetCurrentAIStateName() const;
	bool IsTargetAware() const;
	bool HasTargetLineOfSight() const;
	bool CanAttackTarget() const;
	K4E::Vector3 GetTargetPosition() const;
	float GetDistanceToTarget() const;

	void StopMove();
	void MoveTowards(const K4E::Vector3& targetPos);
	void MoveTowards(const K4E::Vector3& targetPos, float speed);
	void MoveTowardsPath(const K4E::Vector3& targetPos, float speed, float deltaTime);
	void MoveAwayFrom(const K4E::Vector3& targetPos, float speed);
	void MoveStrafeAround(const K4E::Vector3& targetPos, float sign, float speed);
	void MoveToLastSeen(float speed);
	void MoveTacticalAround(const K4E::Vector3& targetPos, float strafeSign, float radialBias, float speed);
	float ChooseBetterStrafeSign(const K4E::Vector3& targetPos, float probeDistance) const;

	void FaceTo(const K4E::Vector3& targetPos);
	void FireAt(const K4E::Vector3& targetPos);
	void UpdateReloadMove(const K4E::Vector3& targetPos, float deltaTime);
	bool CanStartShooting() const;
	void StartReload();

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
	float GetFireRange() const { return combat_.fireRange; }

	float GetMinCombatRange() const { return combat_.minCombatRange; }
	float GetIdealCombatRange() const { return combat_.idealCombatRange; }
	float GetMaxCombatRange() const { return combat_.maxCombatRange; }
	float GetRetreatRange() const { return combat_.retreatRange; }
	float GetIdealRangeMin() const { return combat_.minCombatRange; }
	float GetIdealRangeMax() const { return combat_.maxCombatRange; }
	float GetTooCloseRange() const { return combat_.retreatRange; }
	float GetTooFarRange() const { return combat_.maxCombatRange; }
	float GetSearchDuration() const { return combat_.searchDuration; }
	float GetLosRepositionEvalSec() const { return combat_.losRepositionEvalSec; }
	float GetShootRepositionEvalSec() const { return combat_.shootRepositionEvalSec; }
	float GetShootMaxStaySec() const { return combat_.shootMaxStaySec; }
	float GetLowHpShootStaySec() const { return survival_.lowHpShootStaySec; }

	float GetApproachSpeed() const { return movement_.approachSpeed; }
	float GetRetreatSpeed() const { return movement_.retreatSpeed; }
	float GetStrafeSpeed() const { return movement_.strafeSpeed; }
	float GetSearchMoveSpeed() const { return movement_.searchMoveSpeed; }
	float GetFireInterval() const { return combat_.fireInterval; }
	float GetEnemyBulletSpeed() const { return combat_.enemyBulletSpeed; }
	float GetAccuracyConeRad() const { return combat_.accuracyConeRad; }
	float GetAimErrorAngleRad() const { return combat_.aimErrorAngleRad; }
	int GetBurstCount() const { return combat_.burstCount; }
	float GetBurstInterval() const { return combat_.burstInterval; }
	float GetPostBurstWait() const { return combat_.postBurstWait; }
	int GetMagazineSize() const { return combat_.magazineSize; }
	int GetCurrentAmmo() const { return currentAmmo_; }
	float GetReloadTime() const { return combat_.reloadTime; }
	bool IsReloading() const { return isReloading_; }
	float GetReloadTimer() const { return reloadTimer_; }
	bool IsReloadMoveEnabled() const { return combat_.reloadMoveEnabled; }
	float GetLosProbeDistance() const { return movement_.losProbeDistance; }
	float GetTacticalBlend() const { return movement_.tacticalBlend; }
	float GetShootMicroStrafeSpeed() const { return movement_.shootMicroStrafeSpeed; }
	float GetWanderMoveSpeed() const { return wander_.wanderMoveSpeed; }
	float GetLowHpRetreatDistance() const { return survival_.lowHpRetreatDistance; }
	float GetLowHpReturnDistance() const { return survival_.lowHpReturnDistance; }
	float GetLowHpRetreatSpeedScale() const { return survival_.lowHpRetreatSpeedScale; }
	float GetLowHpShootRange() const { return survival_.lowHpShootRange; }
	float GetRetreatDecisionInterval() const { return survival_.retreatDecisionInterval; }
	float GetRetreatCooldown() const { return survival_.retreatCooldown; }
	float GetLowHpThresholdRate() const { return survival_.lowHpThresholdRate; }
	float GetCoverRepathInterval() const { return cover_.coverRepathInterval; }
	float GetCoverStayTime() const { return cover_.coverStayTime; }
	bool IsLowHp() const { return GetHpRate() <= survival_.lowHpThresholdRate; }
	bool IsRetreating() const { return retreatDecision_.IsRetreating(); }
	bool IsHostileFromDamage() const { return hasDamageStimulus_ && lastDamageTimeSec_ >= 0.0f && (aliveTimeSec_ - lastDamageTimeSec_) <= combat_.searchDuration; }
	float GetLastDamageElapsedSec() const { return (lastDamageTimeSec_ >= 0.0f) ? (aliveTimeSec_ - lastDamageTimeSec_) : -1.0f; }
	bool IsInHitReaction() const { return hitReactionTimer_ > 0.0f; }
	float GetHitReactionMoveWeight() const { return reaction_.hitReactionMoveWeight; }
	float GetEvadeWeight() const { return reaction_.evadeWeight; }
	float GetCoverBias() const { return reaction_.coverBias; }
	float GetCoverPreference() const { return traits_.coverPreference; }
	float GetAggression() const { return traits_.aggression; }
	int GetConsecutiveHitCount() const { return consecutiveHitCount_; }
	EnemyRetreatController::Plan EvaluateRetreatPlan(float distToTarget, bool canShoot);
	EnemyEvadeController::Plan EvaluateEvadePlan(bool canShoot) const;
	[[nodiscard]] bool TryFindCoverPosition(const K4E::Vector3& targetPos, bool preferRetreat, K4E::Vector3& outPosition) const;
	[[nodiscard]] EnemyCoverController::Output EvaluateCoverAction(const K4E::Vector3& targetPos, const K4E::Vector3& coverPos, bool dangerMode, bool hasCover, float deltaTime);
	void ResetCoverAction();
	bool ShouldShootFromCover(const EnemyCoverController::Output& coverAction) const;
	bool IsMovementStuck() const { return isMovementStuck_; }

	void UpdateStrafeDecision(float dt);
	float GetCurrentStrafeSign() const { return currentStrafeSign_; }
	void ForceStrafeSign(float sign) { currentStrafeSign_ = (sign >= 0.0f) ? 1.0f : -1.0f; }
	void UpdateWander(float deltaTime);

	void PlayIdleAnimation();
	void PlayMoveAnimation(float moveSpeed = -1.0f);
	void PlayShootAnimation();
	void PlaySearchAnimation(float moveSpeed = -1.0f);
	void PlayDeadAnimation();

protected: /// ---------- EnemyBaseからの通知 ---------- ///

	// EnemyBaseからの弾ヒット
	void OnBulletHit(K4E::Collider* bulletCollider) override;


private: /// ---------- 視界判定 ---------- ///

	bool CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget);
	bool CanShootTarget(const K4E::Vector3& targetPos) const;
	bool HasLineOfSight(const K4E::Vector3& fromPos, const K4E::Vector3& toPos) const;
	float EvaluateLineOfSightScore(const K4E::Vector3& samplePos, const K4E::Vector3& targetPos) const;

private: /// ---------- 内部処理 ---------- ///

	void MoveInDirectionXZ(const K4E::Vector3& dir, float speed);
	void SetAnimState(AnimState next);
	void UpdateAnimation(float dt);
	void UpdateNavigatorSource();
	void PickNextWanderTarget();
	void TryStepJump(const K4E::Vector3& moveDirection);
	void UpdateTraitProfile();
	void RegisterDamageStimulus(const K4E::Vector3& hitPos, const K4E::Vector3& attackOrigin, const K4E::Vector3& hitDir);
	void UpdateReloadTimer(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	// 外部依存
	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	CollisionManager* collisionManager_ = nullptr;

	EnemyPerceptionConfig perception_{};

	EnemyCombatConfig combat_{};

	EnemySurvivalConfig survival_{};

	EnemyReactionConfig reaction_{};

	EnemyCoverConfig cover_{};

	EnemyMovementConfig movement_{};

	EnemyWanderConfig wander_{};

	EnemyMemory memory_{};

	EnemyFacing facing_{};

	K4E::Vector3 lastDamageHitPos_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 lastDamageDirection_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 lastAttackOrigin_{ 0.0f, 0.0f, 0.0f };
	float lastDamageTimeSec_ = -1.0f;
	float aliveTimeSec_ = 0.0f;
	float lastDeltaTimeSec_ = 0.0f;
	bool hasDamageStimulus_ = false;
	bool suppressNextDamageStimulus_ = false;

	EnemyAStarNavigator navigator_{};

	EnemyCoverSelector coverSelector_{};

	EnemyCoverController coverController_{};

	EnemyRetreatController retreatController_{};
	EnemyRetreatDecisionMemory retreatDecision_{};
	EnemyStuckController stuckController_{};

	EnemyEvadeController evadeController_{};
	
	EnemyAimController aimController_{};

	EnemyTraitProfile traits_{};

	std::unique_ptr<IEnemyState> state_ = nullptr;
	float fireCooldown_ = 0.0f;
	int burstShotsRemaining_ = 0;
	int currentAmmo_ = 0;
	bool isReloading_ = false;
	float reloadTimer_ = 0.0f;
	float strafeDecisionTimer_ = 0.0f;
	float currentStrafeSign_ = 1.0f;
	K4E::Vector3 spawnPosition_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 wanderTarget_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 wanderProbePosition_{ 0.0f, 0.0f, 0.0f };
	float wanderRetargetTimer_ = 0.0f;
	float wanderStuckTimer_ = 0.0f;
	bool hasWanderTarget_ = false;
	float hitReactionTimer_ = 0.0f;
	float jumpCooldownTimer_ = 0.0f;
	float hitChainTimer_ = 0.0f;
	int consecutiveHitCount_ = 0;
	bool moveCommandedThisFrame_ = false;
	bool isMovementStuck_ = false;

	AnimState animState_ = AnimState::Idle;
	float animTime_ = 0.0f;
	float animMoveRate_ = 1.0f;
};
