#include "EnemyAttackState.h"
#include "Enemy.h"
#include "Player.h"
#include "EnemyIdleState.h"
#include <EnemyChaseState.h>


/// -------------------------------------------------------------
///				　		開始処理
/// -------------------------------------------------------------
void EnemyAttackState::Enter(Enemy* enemy)
{
	enemy->SetStateName("Attack");
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void EnemyAttackState::Update(Enemy* enemy)
{
	Vector3 toPlayer = enemy->GetTargetPlayer()->GetWorldPosition() - enemy->GetWorldPosition();
	float distance = Vector3::Length(toPlayer);

	// 攻撃距離外に出たらIdleに戻る
	if (distance > 150.0f)
	{
		enemy->ChangeState(std::make_unique<EnemyIdleState>());
		return;
	}

	if (distance > 120.0f)
	{
		enemy->ChangeState(std::make_unique<EnemyChaseState>());
	}

	// プレイヤーの方向へ回転だけ追従する
	float targetYaw = std::atan2(-toPlayer.x, toPlayer.z);
	float currentYaw = enemy->GetRotate().y;
	float newYaw = Vector3::LerpAngle(currentYaw, targetYaw, 0.2f); // ← 追従の滑らかさ

	enemy->SetRotate({ 0.0f, newYaw, 0.0f });

	// 攻撃処理（弾発射）
	enemy->RequestShoot();
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void EnemyAttackState::Exit(Enemy* enemy)
{
}
