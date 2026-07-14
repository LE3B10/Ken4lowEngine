#include "BossMovementComponent.h"
#include "BossBase.h"
#include <LinearInterpolation.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
	constexpr float kEpsilon = 0.0001f;
}

void BossMovementComponent::Initialize(float moveSpeed, float turnSpeed, float stopDistance)
{
	moveSpeed_ = moveSpeed;
	turnSpeed_ = turnSpeed;
	stopDistance_ = stopDistance;
	characterMovement_ = nullptr;
}

void BossMovementComponent::Finalize()
{
	if (characterMovement_) characterMovement_->Stop();
	characterMovement_ = nullptr; // 共通Movementの所有権はBoss Actor側に残す。
}

void BossMovementComponent::Update(BossBase& boss, float deltaTime)
{
	characterMovement_ = boss.GetCharacterComponent<K4E::CharacterMovementComponent>();
	if (!characterMovement_) return;

	if (boss.IsDead()) { characterMovement_->Stop(); return; }
	if (boss.GetAttackComponent() && boss.GetAttackComponent()->IsAttacking()) { characterMovement_->Stop(); return; }
	if (boss.GetState() != BossState::Move) { characterMovement_->Stop(); return; }

	FaceToTarget(boss, deltaTime);
	MoveTowardsTarget(boss, deltaTime);
}

void BossMovementComponent::FaceToTarget(BossBase& boss, float deltaTime) const
{
	const K4E::Vector3 from = boss.GetPosition();
	const K4E::Vector3 to = boss.GetTargetPosition();
	const float dx = to.x - from.x;
	const float dz = to.z - from.z;
	if (std::fabs(dx) < kEpsilon && std::fabs(dz) < kEpsilon) return;

	const float targetYaw = std::atan2(-dx, dz);
	float currentYaw = boss.GetYaw();
	float deltaYaw = WrapAngle(targetYaw - currentYaw);
	const float maxStep = turnSpeed_ * deltaTime;
	deltaYaw = std::clamp(deltaYaw, -maxStep, maxStep);
	currentYaw += deltaYaw;
	boss.SetYaw(currentYaw);
}

void BossMovementComponent::MoveTowardsTarget(BossBase& boss, float deltaTime)
{
	if (!characterMovement_) return;

	K4E::Vector3 position = boss.GetPosition();
	const K4E::Vector3 target = boss.GetTargetPosition();
	float dx = target.x - position.x;
	float dz = target.z - position.z;
	const float distSq = dx * dx + dz * dz;
	const float stopDistSq = stopDistance_ * stopDistance_;
	if (distSq <= stopDistSq) { characterMovement_->Stop(); return; }

	const float dist = std::sqrt(distSq);
	if (dist < kEpsilon) { characterMovement_->Stop(); return; }

	dx /= dist;
	dz /= dist;
	characterMovement_->SetVelocity({ dx * moveSpeed_, 0.0f, dz * moveSpeed_ });
	const K4E::Vector3 displacement = characterMovement_->CalculateDisplacement(deltaTime);
	position = position + displacement; // Boss固有のWorldCollision経路へ共通Movementの変位だけを渡す。
	boss.SetPosition(position);
}
