#include "BossMovementComponent.h"
#include "BossBase.h"
#include <LinearInterpolation.h>

#include <cmath>
#include <algorithm>
#include <numbers>

using namespace Ken4lowEngine;

namespace
{
	/// ---------- 定数 ---------- ///

	// 角度計算や距離計算で使う定数
	constexpr float kPi = std::numbers::pi_v<float>;
	
	// 2πは角度のラップ処理などで使う
	constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

	// ほとんどゼロに近い値を比較するための定数
	constexpr float kEpsilon = 0.0001f;
}

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossMovementComponent::Initialize(float moveSpeed, float turnSpeed, float stopDistance)
{
	moveSpeed_ = moveSpeed;
	turnSpeed_ = turnSpeed;
	stopDistance_ = stopDistance;
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossMovementComponent::Update(BossBase& boss, float deltaTime)
{
	// 死亡中は移動しない
	if (boss.IsDead()) return;

	// 攻撃中は移動しない
	if (boss.GetAttackComponent() && boss.GetAttackComponent()->IsAttacking()) return;

	// Move中だけ移動を担当する
	if (boss.GetState() != BossState::Move)	return;

	// ターゲット方向へ向く
	FaceToTarget(boss, deltaTime);

	// ターゲットに近づく
	MoveTowardsTarget(boss, deltaTime);
}

/// -------------------------------------------------------------
///					ターゲット方向へ向く
/// -------------------------------------------------------------
void BossMovementComponent::FaceToTarget(BossBase& boss, float deltaTime) const
{
	const K4E::Vector3 from = boss.GetPosition();
	const K4E::Vector3 to = boss.GetTargetPosition();

	// XZ平面での方向ベクトルを計算
	const float dx = to.x - from.x;
	const float dz = to.z - from.z;

	// 目標とする方向がほとんどない場合は回転しない
	if (std::fabs(dx) < kEpsilon && std::fabs(dz) < kEpsilon)
		return;

	// +Z前方想定
	const float targetYaw = std::atan2(-dx, dz);

	// 現在の向き
	float currentYaw = boss.GetYaw();

	// 角度の差を求め、回転速度に基づいて補間
	float deltaYaw = WrapAngle(targetYaw - currentYaw);

	// 角度の差を回転速度で制限
	const float maxStep = turnSpeed_ * deltaTime;

	// 角度の差を回転速度で制限
	deltaYaw = std::clamp(deltaYaw, -maxStep, maxStep);

	// 現在の向きを更新
	currentYaw += deltaYaw;

	// 更新した向きをセット
	boss.SetYaw(currentYaw);
}

/// -------------------------------------------------------------
///						ターゲットへ近づく
/// -------------------------------------------------------------
void BossMovementComponent::MoveTowardsTarget(BossBase& boss, float deltaTime) const
{
	K4E::Vector3 position = boss.GetPosition();
	const K4E::Vector3 target = boss.GetTargetPosition();

	// XZ平面での方向ベクトルを計算
	float dx = target.x - position.x;
	float dz = target.z - position.z;

	// XZ平面での距離を計算
	const float distSq = dx * dx + dz * dz;
	const float stopDistSq = stopDistance_ * stopDistance_;

	// 十分近ければ止まる
	if (distSq <= stopDistSq) return;

	// 距離を正規化して移動量を計算
	const float dist = std::sqrt(distSq);

	// 距離がほとんどない場合は移動しない
	if (dist < kEpsilon) return;

	// 方向ベクトルを正規化
	dx /= dist;
	dz /= dist;

	// 移動量を加算
	position.x += dx * moveSpeed_ * deltaTime;
	position.z += dz * moveSpeed_ * deltaTime;

	// 更新した位置をセット
	boss.SetPosition(position);
}