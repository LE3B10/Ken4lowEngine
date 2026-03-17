#include "BossMovementComponent.h"
#include "Core/BossBase.h"

#include <cmath>
#include <algorithm>

namespace
{
	constexpr float kPi = 3.1415926535f;
	constexpr float kTwoPi = 6.283185307f;
	constexpr float kEpsilon = 0.0001f;

	float Clamp(float value, float minValue, float maxValue)
	{
		return (std::max)(minValue, (std::min)(value, maxValue));
	}

	float WrapAngle(float angle)
	{
		while (angle > kPi)
		{
			angle -= kTwoPi;
		}
		while (angle < -kPi)
		{
			angle += kTwoPi;
		}
		return angle;
	}
}

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossMovementComponent::Initialize(float moveSpeed, float turnSpeed, float stopDistance)
{
	moveSpeed_ = moveSpeed;
	turnSpeed_ = turnSpeed;
	stopDistance_ = stopDistance;
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossMovementComponent::Update(BossBase& boss, float deltaTime)
{
	// 死亡中は移動しない
	if (boss.IsDead())
	{
		return;
	}

	// Move中だけ移動を担当する
	if (boss.GetState() != BossState::Move)
	{
		return;
	}

	FaceToTarget(boss, deltaTime);
	MoveTowardsTarget(boss, deltaTime);
}

/// -------------------------------------------------------------
/// ターゲット方向へ向く
/// -------------------------------------------------------------
void BossMovementComponent::FaceToTarget(BossBase& boss, float deltaTime)
{
	const K4E::Vector3 from = boss.GetPosition();
	const K4E::Vector3 to = boss.GetTargetPosition();

	const float dx = to.x - from.x;
	const float dz = to.z - from.z;

	if (std::fabs(dx) < kEpsilon && std::fabs(dz) < kEpsilon)
	{
		return;
	}

	// +Z前方想定
	const float targetYaw = std::atan2(dx, dz);
	float currentYaw = boss.GetYaw();

	float deltaYaw = WrapAngle(targetYaw - currentYaw);
	const float maxStep = turnSpeed_ * deltaTime;
	deltaYaw = Clamp(deltaYaw, -maxStep, maxStep);

	currentYaw += deltaYaw;
	boss.SetYaw(currentYaw);
}

/// -------------------------------------------------------------
/// ターゲットへ近づく
/// -------------------------------------------------------------
void BossMovementComponent::MoveTowardsTarget(BossBase& boss, float deltaTime)
{
	K4E::Vector3 position = boss.GetPosition();
	const K4E::Vector3 target = boss.GetTargetPosition();

	float dx = target.x - position.x;
	float dz = target.z - position.z;

	const float distSq = dx * dx + dz * dz;
	const float stopDistSq = stopDistance_ * stopDistance_;

	// 十分近ければ止まる
	if (distSq <= stopDistSq)
	{
		return;
	}

	const float dist = std::sqrt(distSq);
	if (dist < kEpsilon)
	{
		return;
	}

	dx /= dist;
	dz /= dist;

	position.x += dx * moveSpeed_ * deltaTime;
	position.z += dz * moveSpeed_ * deltaTime;

	boss.SetPosition(position);
}