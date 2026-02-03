#define NOMINMAX
#include "BossRushState.h"
#include "BossEnemy.h"
#include <Player.h>
#include <LinearInterpolation.h>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

void BossRushState::OnEnter(BossEnemy* boss)
{
	// 方向決定（開始時のプレイヤー位置を見る）
	Player* player = boss->GetPlayer();
	K4E::Vector3 bossPos = boss->GetPosition();
	const auto& turning = boss->GetTurning();

	elapsed_ = 0.0f;
	moved_ = 0.0f;
	const float keepDistance = turning.rush.keepDistance;

	if (player)
	{
		K4E::Vector3 playerPos = player->GetCenterPosition();
		K4E::Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

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
	const auto& turning = boss->GetTurning();
	elapsed_ += deltaTime;

	if (elapsed_ < turning.rush.chargeTime)
	{
		// 溜め中：向きだけキープ
		boss->UpdateFacingDirection(dir_, deltaTime);
	}
	else
	{
		// Rush フェーズ
		const float rushPhaseDuration = turning.rush.duration - turning.rush.chargeTime;
		float rushTime = elapsed_ - turning.rush.chargeTime;
		rushTime = std::clamp(rushTime, 0.0f, rushPhaseDuration);

		float t = (rushPhaseDuration > 0.0f) ? (rushTime / rushPhaseDuration) : 1.0f;

		// 加速→減速のカーブ
		float speedScale = K4E::EaseInOutQuad(t);
		float speed = turning.rush.speed * speedScale;

		// 移動距離制限
		float step = speed * deltaTime;

		// 進み過ぎないように調整
		float remain = moveDistance_ - moved_;
		if (remain <= 0.0f)     step = 0.0f;
		else if (step > remain)	step = remain;

		if (step > 0.0f)
		{

			K4E::Vector3 pos = boss->GetPosition();
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
	if (elapsed_ >= turning.rush.duration || moved_ >= moveDistance_)
	{
		boss->SetAttackCooldown(turning.rush.cooldown);
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
