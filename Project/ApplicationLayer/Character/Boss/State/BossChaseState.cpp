#include "BossChaseState.h"
#include "BossEnemy.h"
#include <Player.h>
#include <cmath>

namespace
{
	// パラメータ
	const float kWalkSpeed = 3.0f; // ふだんの追いかけ速度
	const float kRushDistance = 7.0f; // この距離以上だと「遠い」
	const float kRushFarTime = 1.0f; // 遠い状態が続いた時間で Rush 解禁
	const float kMinDistance = 2.0f; // これ以上は近づかない（プレイヤーとの最小距離）
}

void BossChaseState::OnEnter(BossEnemy* boss)
{
	// 安全確認
	if (!boss) return;

	// 追尾状態開始時の初期化処理があればここに記述
}

BehaviorStatus BossChaseState::Update(BossEnemy* boss, float deltaTime)
{
	Player* player = boss->GetPlayer();

	if (!player) return BehaviorStatus::Running;

	// 位置
	Vector3 bossPos = boss->GetPosition();
	Vector3 playerPos = player->GetCenterPosition();

	// XZ 平面の方向ベクトル
	Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

	float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
	float dist = (distSq > 0.0f) ? std::sqrt(distSq) : 0.0f;

	// 正規化方向
	Vector3 moveDir{ 0.0f, 0.0f, 0.0f };
	if (dist > 0.001f)
	{
		float inv = 1.0f / dist;
		moveDir.x = toPlayer.x * inv;
		moveDir.z = toPlayer.z * inv;
	}

	// 進行方向を向く
	boss->UpdateFacingDirection(moveDir, deltaTime);

	// 歩きで追いかける（近づきすぎないようにクランプ）
	if (dist > kMinDistance && dist > 0.001f)
	{
		float move = kWalkSpeed * deltaTime;
		float excess = dist - kMinDistance; // ここまでなら近づいてOK

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
	if (dist > kRushDistance)
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
