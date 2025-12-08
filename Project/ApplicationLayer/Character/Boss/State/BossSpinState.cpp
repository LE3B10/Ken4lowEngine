#include "BossSpinState.h"
#include "BossEnemy.h"
#include <LinearInterpolation.h>
#include "Player.h"
#include <numbers>

void BossSpinState::OnEnter(BossEnemy* boss)
{
	const auto& turning = boss->GetTurning();

	elapsed_ = 0.0f;
	hasHitPlayer_ = false; // プレイヤーにまだ当たっていない

	// 開始時の向きもここで覚えておくと回転が安定する
	startYaw_ = boss->GetYaw();

	// 近すぎる場合は、少しだけプレイヤーから離れる
	if (Player* player = boss->GetPlayer())
	{
		Vector3 bossPos = boss->GetPosition();
		Vector3 playerPos = player->GetCenterPosition();

		// ボスから見たプレイヤーへのベクトル（Y成分は無視）
		Vector3 toPlayer{ playerPos.x - bossPos.x, 0.0f, playerPos.z - bossPos.z };

		// プレイヤーまでの距離の2乗
		float distSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;

		// 十分離れていない場合
		if (distSq > 0.0001f)
		{
			float dist = std::sqrt(distSq);
			if (dist < turning.spin.safeDistance)
			{
				// 足りない分を計算
				float push = turning.spin.safeDistance - dist;

				// プレイヤーと逆方向へ押し戻す
				float inv = 1.0f / dist;
				Vector3 away{ -toPlayer.x * inv, 0.0f, -toPlayer.z * inv };

				// 位置を更新
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
	const auto& turning = boss->GetTurning();

	elapsed_ += deltaTime;

	// 開始時のヨー角を保存
	float t = (turning.spin.duration > 0.0f) ? (elapsed_ / turning.spin.duration) : 1.0f;

	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	// 緩急のある回転（0→速く→0）
	float eased = EaseInOutQuad(t);

	const float twoPi = 2.0f * std::numbers::pi_v<float>; // 開始時のヨー角を保存（最初のフレームのみ）
	float angleOffset = twoPi * turning.spin.rotations * eased;	  // 最初のフレームで保存

	boss->SetYaw(startYaw_ + angleOffset);

	// ある程度回転が始まってから〜終盤までだけ判定する（0.2〜0.8くらい）
	if (!hasHitPlayer_ && t >= 0.2f && t <= 0.8f)
	{
		if (Player* player = boss->GetPlayer())
		{
			Vector3 bossPos = boss->GetPosition();
			Vector3 playerPos = player->GetCenterPosition();

			// XZ 平面上の距離
			Vector3 diff{
				playerPos.x - bossPos.x,
				0.0f,
				playerPos.z - bossPos.z
			};

			float distSq = diff.x * diff.x + diff.z * diff.z;
			float hitR = turning.spin.hitRadius;
			float hitRSq = hitR * hitR;

			if (distSq <= hitRSq)
			{
				// ---- プレイヤーにヒット！ ----

				// ダメージ（値は kSpinDamage で調整）
				player->TakeDamage(turning.spin.damage);

				// ノックバック方向（ボス→プレイヤー）
				if (distSq > 0.0001f)
				{
					float inv = 1.0f / std::sqrt(distSq);
					Vector3 dir{
						diff.x * inv,
						0.0f,
						diff.z * inv
					};

					player->ApplyDamageImpulse(dir, turning.spin.knockbackH, turning.spin.knockbackUp);
				}

				// 1 回のスピンにつき 1 回だけ
				hasHitPlayer_ = true;
			}
		}
	}

	if (elapsed_ >= turning.spin.duration)
	{
		boss->SetAttackCooldown(turning.spin.cooldown); // 終了後クールタイム設定
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