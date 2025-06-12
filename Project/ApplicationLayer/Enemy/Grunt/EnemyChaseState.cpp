#include "EnemyChaseState.h"
#include "Enemy.h"
#include "Player.h"
#include "EnemyAttackState.h"
#include <EnemyIdleState.h>


/// -------------------------------------------------------------
///				　		開始処理
/// -------------------------------------------------------------
void EnemyChaseState::Enter(Enemy* enemy)
{
	enemy->SetStateName("Chase");
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void EnemyChaseState::Update(Enemy* enemy)
{
	Vector3 toPlayer = enemy->GetTargetPlayer()->GetWorldTransform()->translate_ - enemy->GetWorldPosition();
	float distance = Vector3::Length(toPlayer);

	if (distance < 100.0f)
	{
		enemy->ChangeState(std::make_unique<EnemyAttackState>());
		return;
	}

	if (distance > 150.0f)
	{
		enemy->ChangeState(std::make_unique<EnemyIdleState>());
		return;
	}

	// 移動処理（プレイヤーに向かって近づく）
	Vector3 direction = Vector3::Normalize(toPlayer);
	enemy->SetTranslate(enemy->GetWorldPosition() + direction * 0.25f);

	float yaw = std::atan2(-direction.x, direction.z);
	enemy->SetRotate({ 0.0f, Vector3::LerpAngle(enemy->GetRotate().y, yaw, 0.1f), 0.0f });
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void EnemyChaseState::Exit(Enemy* enemy)
{
}
