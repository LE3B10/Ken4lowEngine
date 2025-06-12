#include "TankIdleState.h"
#include "Player.h"
#include "Enemy.h"

void TankIdleState::Enter(Enemy* enemy)
{
	enemy->SetStateName("TankIdle");
	enemy->SetHP(1000.0f); // タンクのHPを設定

	// 現在位置を基準点として記録
	enemy->SetIdleBasePosition(enemy->GetWorldPosition());
}

void TankIdleState::Update(Enemy* enemy)
{
	idleTimer_ += 1.0f / 60.0f;

	// 最低1秒間は移動のみ行う（攻撃遷移を抑制）
	if (idleTimer_ >= 1.0f) {
		Vector3 toPlayer = enemy->GetTargetPlayer()->GetWorldTransform()->translate_ - enemy->GetWorldPosition();
		float distanceToPlayer = Vector3::Length(toPlayer);
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

void TankIdleState::Exit(Enemy* enemy)
{
}

void TankIdleState::PlayGroundShakeEffect()
{

}
