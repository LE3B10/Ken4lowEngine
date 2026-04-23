#include "Enemy.h"
#include "IEnemyState.h"
#include "EnemyIdleState.h"
#include "EnemyCombatMoveState.h"
#include "EnemyShootState.h"
#include "EnemySearchState.h"
#include "EnemyDeadState.h"

#include <cmath>

using namespace Ken4lowEngine;

void Enemy::Initialize()
{
	EnemyBase::Initialize();

	memory_.lastSeenPos = GetCenterPosition();
	memory_.timeSinceSeen = 9999.0f;

	facing_.yawRad = 0.0f;
	fireCooldown_ = 0.0f;

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

	EnemyBase::Update(deltaTime);
}

void Enemy::ChangeState(std::unique_ptr<IEnemyState> nextState)
{
	// 現在の状態から抜ける
	if (state_) state_->Exit(*this);

	// 新しい状態に入る
	state_ = std::move(nextState);

	// 新しい状態の Enter を呼ぶ
	if (state_) state_->Enter(*this);
}

void Enemy::ChangeStateToIdle()
{
	ChangeState(std::make_unique<EnemyIdleState>());
}

void Enemy::ChangeStateToCombatMove()
{
	ChangeState(std::make_unique<EnemyCombatMoveState>());
}

void Enemy::ChangeStateToShoot()
{
	ChangeState(std::make_unique<EnemyShootState>());
}

void Enemy::ChangeStateToSearch()
{
	ChangeState(std::make_unique<EnemySearchState>());
}

void Enemy::ChangeStateToDead()
{
	ChangeState(std::make_unique<EnemyDeadState>());
}

K4E::Vector3 Enemy::GetTargetPosition() const
{
	// ターゲットがいない場合は自分の中心座標を返す
	if (!target_) return GetCenterPosition();

	// ターゲットの中心座標を返す
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

void Enemy::MoveTowards(const K4E::Vector3& targetPos)
{
	Vector3 direction = targetPos - GetCenterPosition();
	direction.y = 0.0f; // 水平方向のみ

	float lenSq = direction.x * direction.x + direction.z * direction.z;
	if (lenSq < 0.0001f)
	{
		StopMove();
		return; // ほとんど同じ位置なら移動しない
	}

	float len = std::sqrt(lenSq);
	direction.x /= len;
	direction.z /= len;

	K4E::Vector3 v = GetVelocity();
	v.x = direction.x * combat_.moveSpeed;
	v.z = direction.z * combat_.moveSpeed;
	SetVelocity(v);

	FaceTo(targetPos);
}

void Enemy::FaceTo(const K4E::Vector3& targetPos)
{
	K4E::Vector3 dir = targetPos - GetCenterPosition();
	dir.y = 0.0f;

	float lenSq = dir.x * dir.x + dir.z * dir.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	facing_.yawRad = std::atan2(dir.x, dir.z);
	SetOrientation({ 0.0f, facing_.yawRad, 0.0f });
}

void Enemy::FireAt(const K4E::Vector3& targetPos)
{
	(void)targetPos;

	if (fireCooldown_ > 0.0f)
	{
		return;
	}

	if (!bulletManager_)
	{
		return;
	}

	fireCooldown_ = combat_.fireInterval;

	// ここは後で弾発射処理に差し替え
	// bulletManager_->SpawnEnemyBullet(...);
}

void Enemy::OnBulletHit(K4E::Collider* bulletCollider)
{
	EnemyBase::OnBulletHit(bulletCollider);

	if (IsDead())
	{
		ChangeStateToDead();
	}
}

bool Enemy::CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget)
{
	(void)targetPos;
	return distToTarget <= perception_.viewRange;
}

bool Enemy::CanShootTarget(const K4E::Vector3& targetPos) const
{
	const Vector3 selfPos = GetCenterPosition();
	Vector3 diff = targetPos - selfPos;
	diff.y = 0.0f;

	const float distSq = diff.x * diff.x + diff.z * diff.z;
	return distSq <= (combat_.attackRange * combat_.attackRange);
}
