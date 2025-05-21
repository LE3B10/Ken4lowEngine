#include "EnemyIdleState.h"
#include "Enemy.h"
#include "Player.h"
#include "EnemyChaseState.h"
#include "EnemyAttackState.h"

void EnemyIdleState::Enter(Enemy* enemy)
{
	enemy->SetStateName("Idle");

	// 現在位置を基準点として記録
	enemy->SetIdleBasePosition(enemy->GetWorldPosition());

	// 初期移動方向（+X方向）
	enemy->SetIdleMoveDirection({ 1.0f, 0.0f, 0.0f });

	idleTimer_ = 0.0f;
}

void EnemyIdleState::Update(Enemy* enemy)
{
	idleTimer_ += 1.0f / 60.0f;

	// 最低1秒間は移動のみ行う（攻撃遷移を抑制）
	if (idleTimer_ >= 1.0f) {
		Vector3 toPlayer = enemy->GetTargetPlayer()->GetWorldPosition() - enemy->GetWorldPosition();
		float distanceToPlayer = Vector3::Length(toPlayer);

		if (distanceToPlayer < 100.0f) {
			enemy->ChangeState(std::make_unique<EnemyAttackState>());
			return;
		}
	}

	// ---- 左右往復移動処理 ----
	const float maxOffset = 25.0f;
	const float speed = 0.1f;

	Vector3 basePos = enemy->GetIdleBasePosition();
	Vector3 currentPos = enemy->GetWorldPosition();
	Vector3 dir = enemy->GetIdleMoveDirection();

	// 移動
	Vector3 nextPos = currentPos + dir * speed;
	enemy->SetTranslate(nextPos);

	// 基準点からのXオフセットが最大距離を超えたら反転
	float offset = nextPos.x - basePos.x;
	if (std::abs(offset) >= maxOffset) {
		dir.x *= -1.0f;
		enemy->SetIdleMoveDirection(dir);
	}

	// 向きを進行方向に合わせる
	float yaw = std::atan2(-dir.x, dir.z);
	enemy->SetRotate({ 0.0f, yaw, 0.0f });
}

void EnemyIdleState::Exit(Enemy* enemy)
{
}
