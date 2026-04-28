#define NOMINMAX
#include "Enemy.h"
#include "IEnemyState.h"
#include "EnemyIdleState.h"
#include "EnemyCombatMoveState.h"
#include "EnemyShootState.h"
#include "EnemySearchState.h"
#include "EnemyDeadState.h"

#include <BulletManager.h>
#include <CollisionManager.h>
#include <CollisionTypeIdDef.h>
#include <Wireframe.h>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;
	constexpr float kEpsilon = 0.0001f;

	float Clamp(float value, float minVal, float maxVal)
	{
		if (value < minVal) return minVal;
		if (value > maxVal) return maxVal;
		return value;
	}

	float LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	Vector3 NormalizeXZ(const Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len < kEpsilon) return { 0.0f, 0.0f, 0.0f };
		return { v.x / len, 0.0f, v.z / len };
	}

	float LengthSqXZ(const Vector3& v)
	{
		return v.x * v.x + v.z * v.z;
	}

	float DotXZ(const Vector3& v1, const Vector3& v2)
	{
		return v1.x * v2.x + v1.z * v2.z;
	}

	float ToRadians(float degrees)
	{
		return degrees * (kPi / 180.0f);
	}

	float Random01()
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		return dist(engine);
	}

	float RandomRange(float minValue, float maxValue)
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minValue, maxValue);
		return dist(engine);
	}

	float RandomSign()
	{
		thread_local std::mt19937 engine{ std::random_device{}() };
		static thread_local std::bernoulli_distribution dist(0.5);
		return dist(engine) ? 1.0f : -1.0f;
	}

	void Damp(float& value, float target, float speed, float deltaTime)
	{
		const float t = Clamp(speed * deltaTime, 0.0f, 1.0f);
		value += (target - value) * t;
	}

	Vector3 ForwardFromYaw(float yawRad)
	{
		return { std::sin(yawRad), 0.0f, std::cos(yawRad) };
	}

	Vector4 LerpColor(const Vector4& a, const Vector4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}
}

void Enemy::Initialize()
{
	EnemyBase::Initialize();
	navigator_.Reset();
	UpdateTraitProfile();
	EnemyCoverController::Config coverConfig{};
	coverConfig.peekOffset = cover_.peekOffset;
	coverConfig.peekExposeMinSec = cover_.peekExposeMinSec;
	coverConfig.peekExposeMaxSec = cover_.peekExposeMaxSec;
	coverConfig.peekHideMinSec = cover_.peekHideMinSec;
	coverConfig.peekHideMaxSec = cover_.peekHideMaxSec;
	coverController_.SetConfig(coverConfig);

	memory_.lastSeenPos = GetCenterPosition();
	memory_.timeSinceSeen = 9999.0f;

	facing_.yawRad = 0.0f;
	fireCooldown_ = 0.0f;
	strafeDecisionTimer_ = 0.0f;
	currentStrafeSign_ = 1.0f;
	spawnPosition_ = GetCenterPosition();
	wanderTarget_ = spawnPosition_;
	wanderProbePosition_ = spawnPosition_;
	wanderRetargetTimer_ = 0.0f;
	wanderStuckTimer_ = 0.0f;
	hasWanderTarget_ = false;
	hitReactionTimer_ = 0.0f;
	jumpCooldownTimer_ = 0.0f;
	hitChainTimer_ = 0.0f;
	consecutiveHitCount_ = 0;
	moveCommandedThisFrame_ = false;
	isMovementStuck_ = false;
	stuckController_.Reset(GetCenterPosition());
	retreatDecision_.Reset();

	animState_ = AnimState::Idle;
	animTime_ = 0.0f;
	animMoveRate_ = 1.0f;
	InitializeSpawnPresentation();

	ChangeState(std::make_unique<EnemyIdleState>());
}

void Enemy::Update(float deltaTime)
{
	UpdateSpawnPresentation(deltaTime);
	UpdateNavigatorSource();
	moveCommandedThisFrame_ = false;

	if (memory_.timeSinceSeen < 9999.0f) memory_.timeSinceSeen += deltaTime;

	if (hitReactionTimer_ > 0.0f)
	{
		hitReactionTimer_ -= deltaTime;
		if (hitReactionTimer_ < 0.0f) hitReactionTimer_ = 0.0f;
	}

	if (jumpCooldownTimer_ > 0.0f)
	{
		jumpCooldownTimer_ -= deltaTime;
		if (jumpCooldownTimer_ < 0.0f) jumpCooldownTimer_ = 0.0f;
	}
	if (hitChainTimer_ > 0.0f)
	{
		hitChainTimer_ -= deltaTime;
		if (hitChainTimer_ <= 0.0f)
		{
			hitChainTimer_ = 0.0f;
			consecutiveHitCount_ = 0;
		}
	}

	if (fireCooldown_ > 0.0f)
	{
		fireCooldown_ -= deltaTime;
		if (fireCooldown_ < 0.0f) fireCooldown_ = 0.0f;
	}

	if (!spawnPresentation_.active && state_) state_->Update(*this, deltaTime);

	EnemyStuckController::UpdateInput stuckInput{};
	stuckInput.selfPos = GetCenterPosition();
	stuckInput.dt = deltaTime;
	stuckInput.moveCommanded = moveCommandedThisFrame_;
	const auto stuckOutput = stuckController_.Update(stuckInput);
	isMovementStuck_ = stuckOutput.isStuck;
	if (stuckOutput.shouldRepath)
	{
		navigator_.Reset();
		UpdateStrafeDecision(999.0f);
		currentStrafeSign_ *= -1.0f;
	}
	if (stuckOutput.shouldRetryJump)
	{
		Vector3 recoverDir = GetVelocity();
		recoverDir.y = 0.0f;
		if (LengthSqXZ(recoverDir) <= kEpsilon)
		{
			recoverDir = memory_.lastSeenPos - GetCenterPosition();
		}
		TryStepJump(recoverDir);
	}

	if (IsDead()) PlayDeadAnimation();

	UpdateAnimation(deltaTime);
	EnemyBase::Update(deltaTime);
}

void Enemy::Draw()
{
	EnemyBase::Draw();
	DrawSpawnPresentation();
}

void Enemy::DrawShadow()
{
	if (spawnPresentation_.active) return;
	EnemyBase::DrawShadow();
}

void Enemy::ChangeState(std::unique_ptr<IEnemyState> nextState)
{
	if (state_) state_->Exit(*this);

	state_ = std::move(nextState);

	if (state_) state_->Enter(*this);
}

/// ------------------------- 状態遷移関数 ------------------------- ///
void Enemy::ChangeStateToIdle() { ChangeState(std::make_unique<EnemyIdleState>()); }
void Enemy::ChangeStateToCombatMove() { ChangeState(std::make_unique<EnemyCombatMoveState>()); }
void Enemy::ChangeStateToShoot() { ChangeState(std::make_unique<EnemyShootState>()); }
void Enemy::ChangeStateToSearch() { ChangeState(std::make_unique<EnemySearchState>()); }
void Enemy::ChangeStateToDead() { ChangeState(std::make_unique<EnemyDeadState>()); }

K4E::Vector3 Enemy::GetTargetPosition() const
{
	if (!target_) return GetCenterPosition();

	return target_->GetCenterPosition();
}

float Enemy::GetDistanceToTarget() const
{
	if (!target_) return 9999.0f;

	Vector3 diff = target_->GetCenterPosition() - GetCenterPosition();
	return Vector3::Length(diff);
}

void Enemy::StopMove()
{
	Vector3 velocity = GetVelocity();
	velocity.x = 0.0f;
	velocity.z = 0.0f;
	SetVelocity(velocity);
}

void Enemy::MoveInDirectionXZ(const K4E::Vector3& direction, float speed)
{
	const Vector3 normDir = NormalizeXZ(direction);
	if (LengthXZ(normDir) < kEpsilon)
	{
		StopMove();
		return; // 方向がほとんどない場合は移動しない
	}
	moveCommandedThisFrame_ = (speed > 0.05f);

	Vector3 v = GetVelocity();
	v.x = normDir.x * speed;
	v.z = normDir.z * speed;
	SetVelocity(v);
	TryStepJump(direction);
}

void Enemy::MoveTowards(const K4E::Vector3& targetPos)
{
	MoveTowards(targetPos, movement_.approachSpeed);
}

void Enemy::MoveTowards(const K4E::Vector3& targetPos, float speed)
{
	Vector3 direction = targetPos - GetCenterPosition();
	direction.y = 0.0f; // 水平方向のみ
	MoveInDirectionXZ(direction, speed);
	FaceTo(targetPos);
}

void Enemy::MoveTowardsPath(const K4E::Vector3& targetPos, float speed, float deltaTime)
{
	Vector3 waypoint = targetPos;
	const float sampleY = GetCenterPosition().y + 1.0f;
	if (isMovementStuck_)
	{
		navigator_.Reset();
	}
	navigator_.GetNextWaypoint(GetCenterPosition(), targetPos, sampleY, deltaTime, waypoint);

	if (isMovementStuck_)
	{
		Vector3 toTarget = targetPos - GetCenterPosition();
		toTarget.y = 0.0f;
		Vector3 side{ toTarget.z, 0.0f, -toTarget.x };
		side = NormalizeXZ(side);
		if (LengthSqXZ(side) > kEpsilon)
		{
			waypoint += side * movement_.losProbeDistance * currentStrafeSign_;
		}
	}

	Vector3 direction = waypoint - GetCenterPosition();
	direction.y = 0.0f; // 水平方向のみ
	MoveInDirectionXZ(direction, speed);
	FaceTo(targetPos);
}

void Enemy::MoveAwayFrom(const K4E::Vector3& targetPos, float speed)
{
	Vector3 direction = GetCenterPosition() - targetPos;
	direction.y = 0.0f; // 水平方向のみ
	MoveInDirectionXZ(direction, speed);
	FaceTo(targetPos);
}

void Enemy::MoveStrafeAround(const K4E::Vector3& targetPos, float sign, float speed)
{
	Vector3 toTarget = targetPos - GetCenterPosition();
	toTarget.y = 0.0f;
	Vector3 fwd = NormalizeXZ(toTarget);
	if (LengthXZ(fwd) <= kEpsilon)
	{
		StopMove();
		return;
	}

	Vector3 right = { fwd.z, 0.0f, -fwd.x }; // XZ平面での右ベクトル
	MoveInDirectionXZ(right * ((sign >= 0.0f) ? 1.0f : -1.0f), speed);
	FaceTo(targetPos);

}

void Enemy::MoveToLastSeen(float speed)
{
	MoveTowards(memory_.lastSeenPos, speed);
}

void Enemy::MoveTacticalAround(const K4E::Vector3& targetPos, float strafeSign, float radialBias, float speed)
{
	Vector3 toTarget = targetPos - GetCenterPosition();
	toTarget.y = 0.0f;
	Vector3 fwd = NormalizeXZ(toTarget);
	if (LengthSqXZ(fwd) <= kEpsilon)
	{
		StopMove();
		return;
	}

	Vector3 right{ fwd.z, 0.0f, -fwd.x };
	const float clampedBias = Clamp(radialBias, -1.0f, 1.0f);
	Vector3 dir = right * ((strafeSign >= 0.0f) ? 1.0f : -1.0f) + (fwd * clampedBias * movement_.tacticalBlend);

	MoveInDirectionXZ(dir, speed);
	FaceTo(targetPos);
}

float Enemy::ChooseBetterStrafeSign(const K4E::Vector3& targetPos, float probeDistance) const
{
	const Vector3 self = GetCenterPosition();
	Vector3 toTarget = targetPos - self;
	toTarget.y = 0.0f;
	Vector3 fwd = NormalizeXZ(toTarget);
	if (LengthSqXZ(fwd) <= kEpsilon)
	{
		return currentStrafeSign_;
	}

	Vector3 right{ fwd.z, 0.0f, -fwd.x };
	const Vector3 leftCandidate = self - right * probeDistance;
	const Vector3 rightCandidate = self + right * probeDistance;

	const float leftScore = EvaluateLineOfSightScore(leftCandidate, targetPos);
	const float rightScore = EvaluateLineOfSightScore(rightCandidate, targetPos);

	if (std::fabs(leftScore - rightScore) <= 0.001f)
	{
		return currentStrafeSign_;
	}

	return (rightScore > leftScore) ? 1.0f : -1.0f;
}

void Enemy::FaceTo(const K4E::Vector3& targetPos)
{
	K4E::Vector3 dir = targetPos - GetCenterPosition();
	dir.y = 0.0f;

	float lenSq = dir.x * dir.x + dir.z * dir.z;
	if (lenSq <= kEpsilon) return;

	facing_.yawRad = std::atan2(dir.x, dir.z);
	SetOrientation({ 0.0f, -facing_.yawRad, 0.0f });
}

void Enemy::FireAt(const K4E::Vector3& targetPos)
{
	if (fireCooldown_ > 0.0f || !bulletManager_) return;

	Vector3 muzzle = GetCenterPosition();
	muzzle.y += combat_.muzzleHeight;

	Vector3 targetEye = targetPos;
	targetEye.y += perception_.targetEyeHeight;
	EnemyAimController::Input aimInput{};
	aimInput.muzzlePosition = muzzle;
	aimInput.targetPosition = targetEye;
	aimInput.distanceToTarget = std::max(0.01f, GetDistanceToTarget());
	aimInput.movementSpeed = LengthXZ(GetVelocity());
	aimInput.lowHp = IsLowHp();
	aimInput.inHitReaction = IsInHitReaction();
	aimInput.baseSpreadRad = 0.008f;
	aimInput.maxSpreadRad = 0.12f;
	aimInput.distanceSpreadWeight = 0.95f;
	aimInput.moveSpreadWeight = 0.85f;
	aimInput.stressSpreadWeight = 0.7f;
	aimInput.stableBonusWeight = 0.55f;
	aimInput.traits = &traits_;
	const Vector3 aim = aimController_.BuildAimPoint(aimInput);

	Vector3 dir = aim - muzzle;
	const float len = Vector3::Length(dir);
	if (len <= kEpsilon) return;
	dir = dir * (1.0f / len); // 正規化

	// 射撃方向とモデル向きを一致させる
	const Vector3 dirXZ = NormalizeXZ(dir);
	if (LengthSqXZ(dirXZ) > kEpsilon)
	{
		facing_.yawRad = std::atan2(dirXZ.x, dirXZ.z);
		SetOrientation({ 0.0f, facing_.yawRad, 0.0f });
	}
	const Vector3 forward = ForwardFromYaw(facing_.yawRad);

	// 自身コライダー内で弾が湧くと即時衝突しやすいため、前方へ少し押し出す
	constexpr float kMuzzleForwardOffset = 1.2f;
	muzzle = muzzle + forward * kMuzzleForwardOffset;

	fireCooldown_ = combat_.fireInterval * traits_.fireIntervalScale;

	bulletManager_->Spawn(
		muzzle,
		dir,
		combat_.bulletSpeed,
		combat_.bulletDamage,
		combat_.bulletLifeSec,
		GetCenterPosition(),
		GetUniqueID(),
		static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet));
}

void Enemy::OnBulletHit(K4E::Collider* bulletCollider)
{
	if (spawnPresentation_.active) return;
	EnemyBase::OnBulletHit(bulletCollider);

	if (!IsDead())
	{
		hitReactionTimer_ = reaction_.hitReactionTime * traits_.reactionDelayScale;
		hitChainTimer_ = reaction_.hitChainWindow;
		++consecutiveHitCount_;
		UpdateStrafeDecision(999.0f);
	}

	if (IsDead()) ChangeStateToDead();
}

void Enemy::UpdateNavigatorSource()
{
	navigator_.SetWorldAABBs(GetResolvedWorldAABBs());
}

bool Enemy::HasLineOfSight(const K4E::Vector3& fromPos, const K4E::Vector3& toPos) const
{
	if (!collisionManager_ || !perception_.useLOS) return true;

	Segment segment{};
	segment.origin = fromPos;
	segment.diff = toPos - fromPos;

	Collider* hitWorld = nullptr;
	const bool blocked = collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), segment, &hitWorld);
	return !blocked;
}

float Enemy::EvaluateLineOfSightScore(const K4E::Vector3& samplePos, const K4E::Vector3& targetPos) const
{
	Vector3 sampleEye = samplePos;
	sampleEye.y += perception_.eyeHeight;

	Vector3 targetEye = targetPos;
	targetEye.y += perception_.targetEyeHeight;

	if (HasLineOfSight(sampleEye, targetEye))
	{
		return 1.0f;
	}

	const Vector3 toTarget = targetPos - samplePos;
	const float dist = LengthXZ(toTarget);
	const float normalize = std::max(0.1f, perception_.viewRange);
	return 0.15f + Clamp(1.0f - (dist / normalize), 0.0f, 0.35f);
}

bool Enemy::CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget)
{
	if (distToTarget > perception_.viewRange) return false;

	const Vector3 selfPos = GetCenterPosition();
	Vector3 toTarget = targetPos - selfPos;
	const float distXZ = LengthXZ(toTarget);

	// 近距離は視野を甘くして不自然な見失いを防ぐ
	const bool nearBonus = (distXZ <= perception_.nearDetectRadius) ||
		(memory_.timeSinceSeen < perception_.nearLoseRadius && distXZ <= perception_.nearLoseRadius);

	if (!nearBonus)
	{
		Vector3 facing = { std::sin(facing_.yawRad), 0.0f, std::cos(facing_.yawRad) };
		Vector3 toTargetXZ = NormalizeXZ(toTarget);
		const float dot = DotXZ(facing, toTargetXZ);
		const float cosHalf = std::cos(ToRadians(perception_.viewFovDeg * 0.5f));

		if (dot < cosHalf) return false;

		if (perception_.useVerticalFov)
		{
			const float dy = targetPos.y - selfPos.y;
			const float pitch = std::atan2(std::fabs(dy), std::max(0.01f, distXZ));
			if (pitch > ToRadians(perception_.viewFovVerticalDeg * 0.5f)) return false;
		}
	}

	Vector3 eye = selfPos;
	eye.y += perception_.eyeHeight;
	Vector3 targetEye = targetPos;
	targetEye.y += perception_.targetEyeHeight;

	if (!HasLineOfSight(eye, targetEye)) return false;

	return true;
}

bool Enemy::CanShootTarget(const K4E::Vector3& targetPos) const
{
	const Vector3 selfPos = GetCenterPosition();
	Vector3 diff = targetPos - selfPos;
	diff.y = 0.0f;

	const float distSq = diff.x * diff.x + diff.z * diff.z;
	if (distSq > (combat_.fireRange * combat_.fireRange)) return false;

	Vector3 muzzle = selfPos;
	muzzle.y += combat_.muzzleHeight;
	Vector3 targetEye = targetPos;
	targetEye.y += perception_.targetEyeHeight;
	return HasLineOfSight(muzzle, targetEye);
}

EnemyRetreatController::Plan Enemy::EvaluateRetreatPlan(float distToTarget, bool canShoot)
{
	EnemyRetreatController::Input input{};
	input.distanceToTarget = distToTarget;
	input.idealRangeMin = combat_.idealRangeMin;
	input.idealRangeMax = combat_.idealRangeMax;
	input.tooCloseRange = combat_.tooCloseRange;
	input.tooFarRange = combat_.tooFarRange;
	input.lowHpRetreatDistance = survival_.lowHpRetreatDistance;
	input.lowHpReturnDistance = survival_.lowHpReturnDistance;
	input.approachSpeed = movement_.approachSpeed;
	input.retreatSpeed = movement_.retreatSpeed;
	input.strafeSpeed = movement_.strafeSpeed;
	input.lowHpRetreatSpeedScale = survival_.lowHpRetreatSpeedScale;
	input.hitReactionMoveWeight = reaction_.hitReactionMoveWeight + (1.0f - traits_.aggression) * 0.25f;
	EnemyRetreatDecisionMemory::Input retreatInput{};
	retreatInput.dt = 0.0f;
	retreatInput.hpRate = GetHpRate();
	retreatInput.distanceToTarget = distToTarget;
	retreatInput.retreatDistance = survival_.lowHpRetreatDistance;
	retreatInput.returnDistance = survival_.lowHpReturnDistance;
	retreatInput.decisionInterval = survival_.retreatDecisionInterval;
	retreatInput.inHitReaction = IsInHitReaction();
	retreatInput.canShoot = canShoot;
	retreatInput.consecutiveHits = consecutiveHitCount_;
	const bool retreating = retreatDecision_.Update(retreatInput);
	input.isLowHp = retreating;
	input.inHitReaction = IsInHitReaction();
	if (consecutiveHitCount_ < 2)
	{
		input.inHitReaction = false;
	}
	input.canShoot = canShoot;
	return retreatController_.EvaluatePlan(input);
}

EnemyEvadeController::Plan Enemy::EvaluateEvadePlan(bool canShoot) const
{
	EnemyEvadeController::Input input{};
	input.inHitReaction = IsInHitReaction();
	input.lowHp = IsLowHp();
	input.canShoot = canShoot;
	input.consecutiveHits = consecutiveHitCount_;
	input.evadeWeight = reaction_.evadeWeight;
	input.coverBias = reaction_.coverBias;
	input.aggression = traits_.aggression;
	input.coverPreference = traits_.coverPreference;
	return evadeController_.Evaluate(input);
}

bool Enemy::TryFindCoverPosition(const K4E::Vector3& targetPos, bool preferRetreat, K4E::Vector3& outPosition) const
{
	EnemyCoverSelector::Config config{};
	config.eyeHeight = perception_.eyeHeight;
	config.targetEyeHeight = perception_.targetEyeHeight;
	config.viewRange = perception_.viewRange;
	config.useLOS = perception_.useLOS;
	config.coverSearchRadius = cover_.coverSearchRadius;
	config.coverSampleCount = cover_.coverSampleCount;
	config.retreatDistanceScoreWeight = cover_.coverDistanceScoreWeight;
	return coverSelector_.TryFindCoverPosition(config, collisionManager_, GetCenterPosition(), targetPos, preferRetreat, outPosition);
}

EnemyCoverController::Output Enemy::EvaluateCoverAction(const K4E::Vector3& targetPos, const K4E::Vector3& coverPos, bool dangerMode, bool hasCover, float deltaTime)
{
	EnemyCoverController::UpdateInput input{};
	input.selfPos = GetCenterPosition();
	input.targetPos = targetPos;
	input.coverPos = coverPos;
	input.dt = deltaTime;
	input.dangerMode = dangerMode;
	input.hasCover = hasCover;
	const Vector3 toTarget = targetPos - coverPos;
	Vector3 right{ toTarget.z, 0.0f, -toTarget.x };
	if (LengthXZ(right) <= kEpsilon) right = { 1.0f, 0.0f, 0.0f };
	const Vector3 leftPos = coverPos - NormalizeXZ(right) * cover_.peekOffset;
	const Vector3 rightPos = coverPos + NormalizeXZ(right) * cover_.peekOffset;
	input.losLeftScore = EvaluateLineOfSightScore(leftPos, targetPos);
	input.losRightScore = EvaluateLineOfSightScore(rightPos, targetPos);
	return coverController_.Update(input);
}

void Enemy::ResetCoverAction()
{
	coverController_.Reset();
}

bool Enemy::ShouldShootFromCover(const EnemyCoverController::Output& coverAction) const
{
	return coverAction.active && coverAction.exposing && coverAction.shouldShoot;
}

void Enemy::UpdateStrafeDecision(float dt)
{
	strafeDecisionTimer_ -= dt;
	if (strafeDecisionTimer_ > 0.0f) return;

	const float r = Random01();
	const float span = movement_.strafeSwitchMaxSec - movement_.strafeSwitchMinSec;
	strafeDecisionTimer_ = (movement_.strafeSwitchMinSec + span * Clamp(r, 0.0f, 1.0f)) * traits_.strafeSwitchScale;

	currentStrafeSign_ = RandomSign();
}

void Enemy::TryStepJump(const K4E::Vector3& moveDirection)
{
	if (!collisionManager_ || jumpCooldownTimer_ > 0.0f || !grounded_) return;
	if (LengthXZ(moveDirection) <= kEpsilon) return;

	const Vector3 forward = NormalizeXZ(moveDirection);
	const Vector3 origin = GetCenterPosition();
	Segment low{};
	low.origin = origin + Vector3{ 0.0f, 0.3f, 0.0f };
	low.diff = forward * movement_.jumpProbeDistance;

	Segment high{};
	high.origin = origin + Vector3{ 0.0f, movement_.jumpStepHeight + 0.3f, 0.0f };
	high.diff = forward * movement_.jumpProbeDistance;

	Collider* hitLow = nullptr;
	const bool lowBlocked = collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), low, &hitLow);
	if (!lowBlocked) return;

	Collider* hitHigh = nullptr;
	const bool highBlocked = collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kWorld), high, &hitHigh);
	if (highBlocked) return;

	Vector3 v = GetVelocity();
	v.y = movement_.jumpVelocity;
	SetVelocity(v);
	jumpCooldownTimer_ = movement_.jumpCooldown;
}

void Enemy::InitializeSpawnPresentation()
{
	spawnPresentation_ = {};
	spawnPresentation_.active = true;
	spawnPresentation_.timer = 0.0f;
	spawnPresentation_.duration = 1.55f;
	spawnPresentation_.anchor = GetCenterPosition();
	spawnPresentation_.particles.clear();
	spawnPresentation_.particles.reserve(48);

	SetSpawnProtection(true);
	SetCenterPosition(spawnPresentation_.anchor + Vector3{ 0.0f, -0.65f, 0.0f });
	SetVelocity({ 0.0f, 0.0f, 0.0f });
	SetColor({ 0.35f, 0.95f, 1.0f, 0.25f });

	constexpr int kParticleCount = 48;
	for (int i = 0; i < kParticleCount; ++i)
	{
		const float ringT = static_cast<float>(i) / static_cast<float>(kParticleCount);
		const float angle = ringT * 2.0f * kPi;
		const float radius = RandomRange(1.7f, 2.8f);
		const float y = RandomRange(-0.2f, 2.2f);
		Vector3 pos{
			spawnPresentation_.anchor.x + std::cos(angle) * radius,
			spawnPresentation_.anchor.y + y,
			spawnPresentation_.anchor.z + std::sin(angle) * radius
		};
		Vector3 toCenter = spawnPresentation_.anchor - pos;

		SpawnFxParticle particle{};
		particle.maxLife = RandomRange(0.7f, 1.4f);
		particle.life = particle.maxLife * RandomRange(0.2f, 1.0f);
		particle.position = pos;
		particle.velocity = toCenter * (1.0f / std::max(0.1f, particle.maxLife));
		particle.size = RandomRange(0.03f, 0.1f);
		spawnPresentation_.particles.push_back(particle);
	}
}

K4E::Vector3 Enemy::RotateYaw(const K4E::Vector3& v, float yawRad) const
{
	const float c = std::cos(yawRad);
	const float s = std::sin(yawRad);
	return { v.x * c - v.z * s, v.y, v.x * s + v.z * c };
}

void Enemy::UpdateSpawnPresentation(float deltaTime)
{
	if (!spawnPresentation_.active) return;

	spawnPresentation_.timer += deltaTime;
	const float t = Clamp(spawnPresentation_.timer / spawnPresentation_.duration, 0.0f, 1.0f);
	const float eased = t * t * (3.0f - 2.0f * t);
	const float riseOffset = (1.0f - eased) * -0.65f;

	SetCenterPosition(spawnPresentation_.anchor + Vector3{ 0.0f, riseOffset, 0.0f });
	SetVelocity({ 0.0f, 0.0f, 0.0f });
	SetColor(LerpColor({ 0.3f, 0.95f, 1.0f, 0.2f }, { 1.0f, 1.0f, 1.0f, 1.0f }, eased));

	for (auto& particle : spawnPresentation_.particles)
	{
		particle.life -= deltaTime;
		if (particle.life <= 0.0f)
		{
			const float angle = RandomRange(0.0f, 2.0f * kPi);
			const float radius = RandomRange(1.4f, 2.5f);
			const float y = RandomRange(-0.15f, 2.0f);
			particle.position = spawnPresentation_.anchor + Vector3{ std::cos(angle) * radius, y, std::sin(angle) * radius };
			particle.maxLife = RandomRange(0.5f, 1.1f);
			particle.life = particle.maxLife;
			particle.size = RandomRange(0.03f, 0.1f);
			const Vector3 toCenter = spawnPresentation_.anchor - particle.position;
			particle.velocity = toCenter * (1.0f / std::max(0.1f, particle.maxLife));
		}
		particle.position = particle.position + particle.velocity * deltaTime;
	}

	if (t >= 1.0f)
	{
		spawnPresentation_.active = false;
		SetSpawnProtection(false);
		SetCenterPosition(spawnPresentation_.anchor);
		SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	}
}

void Enemy::DrawSpawnPresentation() const
{
	if (!spawnPresentation_.active) return;

	auto* wire = Wireframe::GetInstance();
	const float t = Clamp(spawnPresentation_.timer / spawnPresentation_.duration, 0.0f, 1.0f);
	const float fade = 1.0f - t;
	const Vector4 accentColor{ 0.36f, 0.92f, 1.0f, 0.88f * fade + 0.12f };
	const Vector4 gridColor{ 0.48f, 0.76f, 1.0f, 0.55f * fade + 0.15f };
	const Vector4 particleColor{ 0.88f, 0.95f, 1.0f, 0.72f * fade + 0.2f };

	const Vector3 floorCenter = spawnPresentation_.anchor + Vector3{ 0.0f, 0.02f, 0.0f };
	constexpr float kGroundRadius = 1.6f;
	constexpr int kGroundGrid = 5;
	for (int i = -kGroundGrid; i <= kGroundGrid; ++i)
	{
		const float p = static_cast<float>(i) / static_cast<float>(kGroundGrid);
		const float x = p * kGroundRadius;
		const float z = p * kGroundRadius;
		wire->DrawLine(floorCenter + Vector3{ x, 0.0f, -kGroundRadius }, floorCenter + Vector3{ x, 0.0f, kGroundRadius }, gridColor);
		wire->DrawLine(floorCenter + Vector3{ -kGroundRadius, 0.0f, z }, floorCenter + Vector3{ kGroundRadius, 0.0f, z }, gridColor);
	}

	const float pulse = 0.72f + std::sin(spawnPresentation_.timer * 6.0f) * 0.28f;
	const Vector3 markerExtents{ kGroundRadius * pulse, 0.0f, kGroundRadius * pulse };
	const Vector3 c0 = floorCenter + Vector3{ -markerExtents.x, 0.0f, -markerExtents.z };
	const Vector3 c1 = floorCenter + Vector3{ markerExtents.x, 0.0f, -markerExtents.z };
	const Vector3 c2 = floorCenter + Vector3{ markerExtents.x, 0.0f, markerExtents.z };
	const Vector3 c3 = floorCenter + Vector3{ -markerExtents.x, 0.0f, markerExtents.z };
	wire->DrawLine(c0, c1, accentColor);
	wire->DrawLine(c1, c2, accentColor);
	wire->DrawLine(c2, c3, accentColor);
	wire->DrawLine(c3, c0, accentColor);

	constexpr int kFrameCount = 4;
	for (int i = 0; i < kFrameCount; ++i)
	{
		const float fi = static_cast<float>(i);
		const float frameT = fi / static_cast<float>(kFrameCount - 1);
		const float rot = spawnPresentation_.timer * (1.9f + fi * 0.42f) + frameT * kPi * 0.7f;
		const float size = 0.55f + fi * 0.45f - t * 0.3f;
		const float y = spawnPresentation_.anchor.y + 0.35f + fi * 0.52f;
		const Vector4 frameColor = LerpColor(accentColor, gridColor, frameT);

		const Vector3 baseCorners[4] = { { -size, 0.0f, -size }, { size, 0.0f, -size }, { size, 0.0f, size }, { -size, 0.0f, size } };
		Vector3 worldCorners[4];
		for (int c = 0; c < 4; ++c)
		{
			worldCorners[c] = spawnPresentation_.anchor + RotateYaw(baseCorners[c], rot);
			worldCorners[c].y = y;
		}
		for (int e = 0; e < 4; ++e)
		{
			wire->DrawLine(worldCorners[e], worldCorners[(e + 1) % 4], frameColor);
		}
		if (i > 0)
		{
			const float prevSize = 0.55f + (fi - 1.0f) * 0.45f - t * 0.3f;
			const float prevRot = spawnPresentation_.timer * (1.9f + (fi - 1.0f) * 0.42f) + (frameT - (1.0f / static_cast<float>(kFrameCount - 1))) * kPi * 0.7f;
			const float prevY = spawnPresentation_.anchor.y + 0.35f + (fi - 1.0f) * 0.52f;
			for (int c = 0; c < 4; ++c)
			{
				const float sx = (c == 0 || c == 3) ? -prevSize : prevSize;
				const float sz = (c < 2) ? -prevSize : prevSize;
				Vector3 prev = spawnPresentation_.anchor + RotateYaw({ sx, 0.0f, sz }, prevRot);
				prev.y = prevY;
				wire->DrawLine(prev, worldCorners[c], frameColor);
			}
		}
	}

	for (const auto& particle : spawnPresentation_.particles)
	{
		const float pt = Clamp(particle.life / std::max(0.01f, particle.maxLife), 0.0f, 1.0f);
		const Vector4 c = LerpColor(gridColor, particleColor, 1.0f - pt);
		const Vector3 s0 = particle.position + Vector3{ -particle.size, 0.0f, -particle.size };
		const Vector3 s1 = particle.position + Vector3{ particle.size, 0.0f, -particle.size };
		const Vector3 s2 = particle.position + Vector3{ particle.size, 0.0f, particle.size };
		const Vector3 s3 = particle.position + Vector3{ -particle.size, 0.0f, particle.size };
		wire->DrawLine(s0, s1, c);
		wire->DrawLine(s1, s2, c);
		wire->DrawLine(s2, s3, c);
		wire->DrawLine(s3, s0, c);
	}
}

void Enemy::UpdateTraitProfile()
{
	traits_.reactionDelayScale = RandomRange(0.9f, 1.12f);
	traits_.strafeSwitchScale = RandomRange(0.88f, 1.2f);
	traits_.fireIntervalScale = RandomRange(0.9f, 1.18f);
	traits_.aggression = RandomRange(0.4f, 0.72f);
	traits_.coverPreference = RandomRange(0.38f, 0.78f);
	traits_.aimStability = RandomRange(0.35f, 0.76f);

	const float cautiousness = 1.0f - traits_.aggression;
	movement_.strafeSwitchMinSec *= RandomRange(0.92f, 1.08f);
	movement_.strafeSwitchMaxSec *= RandomRange(0.94f, 1.14f);
	combat_.fireInterval *= (0.95f + cautiousness * 0.08f);
	reaction_.coverBias = Clamp(reaction_.coverBias + (traits_.coverPreference - 0.5f) * 0.25f, 0.45f, 0.82f);
	reaction_.evadeWeight = Clamp(reaction_.evadeWeight + cautiousness * 0.08f, 0.58f, 0.84f);
}

void Enemy::PickNextWanderTarget()
{
	const Vector3 center = spawnPosition_;
	Vector3 selected = center;
	bool found = false;

	for (int i = 0; i < 8; ++i)
	{
		const float angle = RandomRange(0.0f, kPi * 2.0f);
		const float radius = RandomRange(wander_.minTargetDistance, wander_.roamRadius);
		Vector3 candidate{
			center.x + std::cos(angle) * radius,
			GetCenterPosition().y,
			center.z + std::sin(angle) * radius
		};

		if (Vector3::Length(candidate - GetCenterPosition()) < wander_.minTargetDistance)
		{
			continue;
		}

		selected = candidate;
		found = true;
		break;
	}

	if (!found)
	{
		const float angle = RandomRange(0.0f, kPi * 2.0f);
		selected.x = center.x + std::cos(angle) * wander_.minTargetDistance;
		selected.y = GetCenterPosition().y;
		selected.z = center.z + std::sin(angle) * wander_.minTargetDistance;
	}

	wanderTarget_ = selected;
	hasWanderTarget_ = true;
	wanderRetargetTimer_ = RandomRange(wander_.retargetIntervalMin, wander_.retargetIntervalMax);
	wanderStuckTimer_ = wander_.stuckCheckInterval;
	wanderProbePosition_ = GetCenterPosition();
}

void Enemy::UpdateWander(float deltaTime)
{
	if (!hasWanderTarget_)
	{
		spawnPosition_ = GetCenterPosition();
		PickNextWanderTarget();
	}

	wanderRetargetTimer_ -= deltaTime;
	wanderStuckTimer_ -= deltaTime;

	const Vector3 self = GetCenterPosition();
	const Vector3 toTarget = wanderTarget_ - self;
	const float targetDist = LengthXZ(toTarget);

	if (targetDist <= wander_.reachDistance || wanderRetargetTimer_ <= 0.0f)
	{
		PickNextWanderTarget();
	}

	if (wanderStuckTimer_ <= 0.0f)
	{
		const float moved = LengthXZ(self - wanderProbePosition_);
		wanderProbePosition_ = self;
		wanderStuckTimer_ = wander_.stuckCheckInterval;
		if (moved < wander_.stuckDistance)
		{
			PickNextWanderTarget();
		}
	}

	MoveTowardsPath(wanderTarget_, wander_.wanderMoveSpeed, deltaTime);
	PlayMoveAnimation(wander_.wanderMoveSpeed);
}

void Enemy::SetAnimState(AnimState next)
{
	if (animState_ == next) return;
	animState_ = next;
	animTime_ = 0.0f;
}

void Enemy::PlayIdleAnimation() { SetAnimState(AnimState::Idle); }
void Enemy::PlayMoveAnimation(float moveSpeed)
{
	SetAnimState(AnimState::Move);
	if (moveSpeed > 0.0f)
	{
		animMoveRate_ = Clamp(moveSpeed / std::max(0.1f, movement_.approachSpeed), 0.7f, 1.6f);
	}
}
void Enemy::PlayShootAnimation() { SetAnimState(AnimState::Shoot); }
void Enemy::PlaySearchAnimation(float moveSpeed)
{
	SetAnimState(AnimState::Search);
	if (moveSpeed > 0.0f)
	{
		animMoveRate_ = Clamp(moveSpeed / std::max(0.1f, movement_.searchMoveSpeed), 0.7f, 1.4f);
	}
}
void Enemy::PlayDeadAnimation() { SetAnimState(AnimState::Dead); }

void Enemy::UpdateAnimation(float dt)
{
	if (IsDead()) return;

	animTime_ += dt;

	auto& body = GetBody();
	auto& parts = GetBodyParts();
	const auto& idx = GetPartIndices();

	if (parts.size() < 5)
	{
		return;
	}

	float targetBodyPitch = 0.0f;
	float targetBodyRoll = 0.0f;
	float targetHeadYaw = 0.0f;
	float targetHeadPitch = 0.0f;
	float targetLeftArmX = -0.05f;
	float targetRightArmX = -0.05f;
	float targetLeftArmZ = 0.0f;
	float targetRightArmZ = 0.0f;
	float targetLeftLegX = 0.0f;
	float targetRightLegX = 0.0f;

	const float walkTime = animTime_ * (5.5f * animMoveRate_);
	const float swing = std::sin(walkTime) * 0.55f;

	switch (animState_)
	{
	case AnimState::Idle:
	{
		const float breath = std::sin(animTime_ * 2.1f);
		targetBodyPitch = 0.04f * breath;
		targetHeadPitch = -0.03f * breath;
		targetHeadYaw = std::sin(animTime_ * 1.3f) * 0.04f;
		break;
	}
	case AnimState::Move:
	case AnimState::Search:
	{
		targetBodyPitch = 0.06f + std::fabs(std::sin(walkTime)) * 0.04f;
		targetBodyRoll = std::sin(walkTime) * 0.06f;
		targetLeftArmX = swing;
		targetRightArmX = -swing;
		targetLeftLegX = -swing;
		targetRightLegX = swing;
		targetLeftArmZ = -targetBodyRoll;
		targetRightArmZ = targetBodyRoll;
		break;
	}
	case AnimState::Shoot:
	{
		const float pulse = std::sin(animTime_ * 18.0f);
		targetBodyPitch = 0.1f;
		targetHeadPitch = -0.05f;
		targetLeftArmX = -0.25f;
		targetRightArmX = -0.9f + pulse * 0.08f;
		targetLeftArmZ = 0.2f;
		targetRightArmZ = -0.08f;
		targetLeftLegX = -0.12f;
		targetRightLegX = 0.12f;
		break;
	}
	case AnimState::Dead:
	default:
	break;
	}

	Damp(body.transform.rotate_.x, targetBodyPitch, 8.0f, dt);
	Damp(body.transform.rotate_.z, targetBodyRoll, 8.0f, dt);

	Damp(parts[idx.head].transform.rotate_.x, targetHeadPitch, 7.0f, dt);
	Damp(parts[idx.head].transform.rotate_.y, targetHeadYaw, 7.0f, dt);

	Damp(parts[idx.leftArm].transform.rotate_.x, targetLeftArmX, 10.0f, dt);
	Damp(parts[idx.rightArm].transform.rotate_.x, targetRightArmX, 10.0f, dt);
	Damp(parts[idx.leftArm].transform.rotate_.z, targetLeftArmZ, 10.0f, dt);
	Damp(parts[idx.rightArm].transform.rotate_.z, targetRightArmZ, 10.0f, dt);

	Damp(parts[idx.leftLeg].transform.rotate_.x, targetLeftLegX, 10.0f, dt);
	Damp(parts[idx.rightLeg].transform.rotate_.x, targetRightLegX, 10.0f, dt);
}
