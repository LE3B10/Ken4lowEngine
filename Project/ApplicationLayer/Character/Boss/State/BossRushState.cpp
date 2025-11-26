#define NOMINMAX
#include "BossRushState.h"
#include "BossEnemy.h"
#include <Player.h>
#include <LinearInterpolation.h>
#include <cmath>

namespace
{
	// Rush 用パラメータ
	const float kRushDuration = 2.0f;	 // 全体時間
	const float kRushChargeTime = 0.5f;  // 溜め時間
	const float kRushSpeed = 250.0f;	 // 最大速度
	const float kRushCooldown = 3.0f;	 // 終了後クールタイム
}

void BossRushState::OnEnter(BossEnemy* boss)
{
	elapsed_ = 0.0f;

	// 方向決定（開始時のプレイヤー位置を見る）
	Player* player = boss->GetPlayer();
	Vector3 bossPos = boss->GetPosition();

	float keepDistance = -10.0f; // プレイヤーを通り過ぎるようにする距離調整

	if (player)
	{
		Vector3 playerPos = player->GetCenterPosition();
		Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

		float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
		if (distSq > 0.0001f)
		{
			float dist = std::sqrt(distSq);
			float inv = 1.0f / std::sqrt(distSq);
			dir_.x = toPlayer.x * inv;
			dir_.z = toPlayer.z * inv;

			moveDistance_ = std::max(dist - keepDistance, 0.0f);
		}
		else
		{
			dir_ = { 0.0f, 0.0f, 1.0f }; // 適当な前方向
			moveDistance_ = 0.0f;
		}
	}
	else
	{
		dir_ = { 0.0f, 0.0f, 1.0f };
		moveDistance_ = 0.0f;
	}

	// すぐに向きを合わせておく
	boss->UpdateFacingDirection(dir_, 0.0f);

	OutputDebugStringA("BossRushState::OnEnter\n");
}

BehaviorStatus BossRushState::Update(BossEnemy* boss, float deltaTime)
{
	elapsed_ += deltaTime;

	if (elapsed_ < kRushChargeTime)
	{
		// 溜め中：向きだけキープ
		boss->UpdateFacingDirection(dir_, deltaTime);
	}
	else
	{
		// Rush フェーズ
		const float rushPhaseDuration = kRushDuration - kRushChargeTime;
		float rushTime = elapsed_ - kRushChargeTime;
		rushTime = std::clamp(rushTime, 0.0f, rushPhaseDuration);

		float t = (rushPhaseDuration > 0.0f) ? (rushTime / rushPhaseDuration) : 1.0f;

		// 加速→減速のカーブ
		float speedScale = EaseInOutQuad(t);
		float speed = kRushSpeed * speedScale;

		// 移動距離制限
		float step = speed * deltaTime;

		// 進み過ぎないように調整
		float remain = moveDistance_ - moved_;
		if (remain <= 0.0f)     step = 0.0f;
		else if (step > remain)	step = remain;

		if (step > 0.0f)
		{

			Vector3 pos = boss->GetPosition();
			pos.x += dir_.x * speed * deltaTime;
			pos.z += dir_.z * speed * deltaTime;
			boss->SetPosition(pos);

			// 進んだ距離を加算
			moved_ += step;
		}

		// 向き更新
		boss->UpdateFacingDirection(dir_, deltaTime);
	}

	// 終了判定（時間 or 距離を走り切ったあと）
	if (elapsed_ >= kRushDuration || moved_ >= moveDistance_)
	{
		boss->SetAttackCooldown(kRushCooldown);
		boss->SetFarFromPlayerTimer(0.0f);

		OutputDebugStringA("BossRushState::End\n");
		return BehaviorStatus::Success;
	}

	return BehaviorStatus::Running;
}

void BossRushState::OnExit(BossEnemy* boss)
{
	(void)boss; // 未使用
}
