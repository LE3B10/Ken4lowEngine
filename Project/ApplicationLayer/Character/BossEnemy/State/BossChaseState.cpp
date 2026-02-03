#include "BossChaseState.h"
#include "BossEnemy.h"
#include <Player.h>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

void BossChaseState::OnEnter(BossEnemy* boss)
{
	// 安全確認
	if (!boss) return;

	// 追尾状態開始時の初期化処理があればここに記述
}

BehaviorStatus BossChaseState::Update(BossEnemy* boss, float deltaTime)
{
	Player* player = boss->GetPlayer();

	const auto& turning = boss->GetTurning();

	if (!player) return BehaviorStatus::Running;

	// 位置
	K4E::Vector3 bossPos = boss->GetPosition();
	K4E::Vector3 playerPos = player->GetCenterPosition();

	// XZ 平面の方向ベクトル
	K4E::Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

	float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
	float dist = (distSq > 0.0f) ? std::sqrt(distSq) : 0.0f;

	// 正規化方向
	K4E::Vector3 moveDir{ 0.0f, 0.0f, 0.0f };
	if (dist > 0.001f)
	{
		float inv = 1.0f / dist;
		moveDir.x = toPlayer.x * inv;
		moveDir.z = toPlayer.z * inv;
	}

	// 進行方向を向く
	boss->UpdateFacingDirection(moveDir, deltaTime);

	// 歩きで追いかける（近づきすぎないようにクランプ）
	if (dist > turning.chase.minDistance && dist > 0.001f)
	{
		float move = turning.chase.walkSpeed * deltaTime;
		float excess = dist - turning.chase.minDistance; // ここまでなら近づいてOK

		if (move > excess) move = excess;

		bossPos.x += moveDir.x * move;
		bossPos.z += moveDir.z * move;
		boss->SetPosition(bossPos);
	}

	// クールタイムを減らす
	float cd = boss->GetAttackCooldown();
	if (cd > 0.0f)
	{
		cd -= deltaTime;
		if (cd < 0.0f) cd = 0.0f;
		boss->SetAttackCooldown(cd);
	}

	// 「遠い状態が続いている時間」を更新（Rush 用）
	float farT = boss->GetFarFromPlayerTimer();
	if (dist > turning.chase.rushDistance)
	{
		farT += deltaTime;
	}
	else
	{
		farT = 0.0f;
	}
	boss->SetFarFromPlayerTimer(farT);

	// 攻撃に入るかどうかの判断は BossEnemy::DecideNextAttackKind に任せる
	// ここでは常に Running を返す
	return BehaviorStatus::Running;
}

void BossChaseState::OnExit(BossEnemy* boss)
{
	// 追尾状態終了時の終了処理があればここに記述
	(void)boss;
}
