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

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <numbers>

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

	void Damp(float& value, float target, float speed, float deltaTime)
	{
		const float t = Clamp(speed * deltaTime, 0.0f, 1.0f);
		value += (target - value) * t;
	}

	Vector3 ForwardFromYaw(float yawRad)
	{
		return { std::sin(yawRad), 0.0f, std::cos(yawRad) };
	}
}

void Enemy::Initialize()
{
	EnemyBase::Initialize();

	memory_.lastSeenPos = GetCenterPosition();
	memory_.timeSinceSeen = 9999.0f;

	facing_.yawRad = 0.0f;
	fireCooldown_ = 0.0f;
	strafeDecisionTimer_ = 0.0f;
	currentStrafeSign_ = 1.0f;

	animState_ = AnimState::Idle;
	animTime_ = 0.0f;
	animMoveRate_ = 1.0f;

	ChangeState(std::make_unique<EnemyIdleState>());
}

void Enemy::Update(float deltaTime)
{
	if (memory_.timeSinceSeen < 9999.0f) memory_.timeSinceSeen += deltaTime;

	if (fireCooldown_ > 0.0f)
	{
		fireCooldown_ -= deltaTime;
		if (fireCooldown_ < 0.0f) fireCooldown_ = 0.0f;
	}

	if (state_) state_->Update(*this, deltaTime);

	if (IsDead()) PlayDeadAnimation();

	UpdateAnimation(deltaTime);
	EnemyBase::Update(deltaTime);
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

	Vector3 v = GetVelocity();
	v.x = normDir.x * speed;
	v.z = normDir.z * speed;
	SetVelocity(v);
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

	Vector3 aim = targetPos;
	aim.y += perception_.targetEyeHeight;

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

	fireCooldown_ = combat_.fireInterval;

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
	EnemyBase::OnBulletHit(bulletCollider);

	if (IsDead())
	{
		ChangeStateToDead();
	}
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
	if (distSq > (combat_.attackRange * combat_.attackRange)) return false;

	Vector3 muzzle = selfPos;
	muzzle.y += combat_.muzzleHeight;
	Vector3 targetEye = targetPos;
	targetEye.y += perception_.targetEyeHeight;
	return HasLineOfSight(muzzle, targetEye);
}

void Enemy::UpdateStrafeDecision(float dt)
{
	strafeDecisionTimer_ -= dt;
	if (strafeDecisionTimer_ > 0.0f) return;

	const float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	const float span = movement_.strafeSwitchMaxSec - movement_.strafeSwitchMinSec;
	strafeDecisionTimer_ = movement_.strafeSwitchMinSec + span * Clamp(r, 0.0f, 1.0f);

	const float side = (std::rand() & 1) ? 1.0f : -1.0f;
	currentStrafeSign_ = side;
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