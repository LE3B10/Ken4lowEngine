#include "BossSpinState.h"
#include "BossEnemy.h"
#include <LinearInterpolation.h>
#include "Player.h"
#include <numbers>

namespace
{
	const float kSpinDuration = 0.7f;  // 回転攻撃の時間
	const float kSpinRotations = 1.5f;  // 1.5 回転くらい
	const float kSpinCooldown = 1.0f;  // 終了後クールタイム
	const float kSpinSafeDistance = 2.0f; // スピン開始時にこれくらいは距離を空けたい
}

void BossSpinState::OnEnter(BossEnemy* boss)
{
	elapsed_ = 0.0f;

	// 近すぎる場合は、少しだけプレイヤーから離れる
	if (Player* player = boss->GetPlayer())
	{
		Vector3 bossPos = boss->GetPosition();
		Vector3 playerPos = player->GetCenterPosition();

		Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

		float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
		if (distSq > 0.0001f)
		{
			float dist = std::sqrt(distSq);
			if (dist < kSpinSafeDistance)
			{
				float push = kSpinSafeDistance - dist;

				// プレイヤーと逆方向へ押し戻す
				float inv = 1.0f / dist;
				Vector3 away{
					-toPlayer.x * inv,
					0.0f,
					-toPlayer.z * inv
				};

				bossPos.x += away.x * push;
				bossPos.z += away.z * push;
				boss->SetPosition(bossPos);
			}
		}
	}

	OutputDebugStringA("BossSpinState::OnEnter\n");
}

BehaviorStatus BossSpinState::Update(BossEnemy* boss, float deltaTime)
{
	elapsed_ += deltaTime;

	float t = (kSpinDuration > 0.0f) ? (elapsed_ / kSpinDuration) : 1.0f;
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// 緩急のある回転（0→速く→0）
	float eased = EaseInOutQuad(t);

	const float twoPi = 2.0f * std::numbers::pi_v<float>;
	float angleOffset = twoPi * kSpinRotations * eased;

	boss->SetYaw(startYaw_ + angleOffset);

	// TODO: ここでプレイヤーとの距離を見て、近ければダメージ処理を追加する

	if (elapsed_ >= kSpinDuration)
	{
		boss->SetAttackCooldown(kSpinCooldown);
		OutputDebugStringA("BossSpinState::End\n");
		return BehaviorStatus::Success;
	}

	return BehaviorStatus::Running;
}

void BossSpinState::OnExit(BossEnemy* boss)
{
	// 向きを開始角に戻すかは好みで
	// boss.SetYaw(startYaw_);
	(void)boss; // 未使用警告回避
}