#include "EnemyAIWanderState.h"
#include "Enemy.h"
#include "Player.h"
#include "EnemyAIChaseState.h"

#include <vector>
#include <random>

namespace K4E = ::Ken4lowEngine;

void EnemyAIWanderState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// 初期状態はWanderに設定
	enemy->SetState(AIState::Wander);
}

void EnemyAIWanderState::Update(Enemy* enemy, float deltaTime)
{
	using BodyPart = BaseCharacter::BodyPart;
	using AIState = Enemy::AIState;
	using WanderInfo = Enemy::WanderInfo;

	//AIState& state = enemy->GetCurrentState();
	BodyPart& body = enemy->GetBody();
	std::vector<BodyPart>& parts = enemy->GetBodyParts();
	WanderInfo& wander = enemy->GetWanderInfo();
	Player* player = enemy->GetPlayer();

	const uint32_t leftArmIndex = enemy->GetPartIndices().leftArm;
	const uint32_t rightArmIndex = enemy->GetPartIndices().rightArm;

	using AttackInfo = Enemy::AttackInfo;
	AttackInfo& attack = enemy->GetAttackInfo();

	// どのステートでもクールダウンは進めたいので、ここで減らす
	if (attack.cooldownTimer > 0.0f) {
		attack.cooldownTimer -= deltaTime;
		if (attack.cooldownTimer < 0.0f) attack.cooldownTimer = 0.0f;
	}

	// 残り時間を減らして、0以下なら新しい向きを決める
	wander.timer -= deltaTime;
	if (wander.timer <= 0.0f) PickNewWanderDirection(enemy);

	// 現在yawを wanderTargetYaw_ にゆっくり近づける（最短回頭）
	float& yawNow = body.transform.rotate_.y;
	float  yawDst = wander.targetYaw;

	float twoPi = 2.0f * std::numbers::pi_v<float>;
	float diff = std::fmod(yawDst - yawNow, twoPi);
	if (diff > std::numbers::pi_v<float>) diff += twoPi;
	if (diff < -std::numbers::pi_v<float>) diff -= twoPi;

	float maxTurn = wander.turnSpeed * deltaTime;
	if (std::fabs(diff) <= maxTurn)
	{
		yawNow = yawDst;
	}
	else
	{
		yawNow += (diff > 0.0f ? +maxTurn : -maxTurn);
	}

	// いま向いてる方向に歩く
	float yaw = body.transform.rotate_.y;
	K4E::Vector3 forward = { -std::sinf(yaw), 0.0f, std::cosf(yaw) };
	K4E::Vector3 before = body.transform.translate_;
	body.transform.translate_ += forward * wander.walkSpeed;

	// ほとんど動けなかったら詰まってると判断→すぐ方向再抽選
	K4E::Vector3 moved = body.transform.translate_ - before;
	float movedLenSq = moved.x * moved.x + moved.z * moved.z;
	if (movedLenSq < (wander.stuckThreshold * wander.stuckThreshold))
	{
		PickNewWanderDirection(enemy);
	}


	// プレイヤー生存＆近距離ならChaseへ移行するロジックは今まで通りでOK
	if (player && !player->IsDeadNow())
	{
		K4E::Vector3 toPlayer = player->GetCenterPosition() - body.transform.translate_;
		float distToPlayer = K4E::Vector3::Length(toPlayer);
		if (distToPlayer <= wander.detectRadius)
		{
			enemy->ResetStateTimer();
			enemy->ChangeState(std::make_unique<EnemyAIChaseState>());

			return;
		}
	}

	// 徘徊時の腕ポーズはゾンビのidle角度にしておく
	float idleRad = wander.idlePoseAngleDeg * std::numbers::pi_v<float> / 180.0f;
	if (leftArmIndex < parts.size() && parts[leftArmIndex].object)
	{
		parts[leftArmIndex].transform.rotate_.x = idleRad;
	}
	if (rightArmIndex < parts.size() && parts[rightArmIndex].object)
	{
		parts[rightArmIndex].transform.rotate_.x = idleRad;
	}
}

void EnemyAIWanderState::Exit(Enemy* enemy)
{
	(void)enemy;
}

void EnemyAIWanderState::PickNewWanderDirection(Enemy* enemy)
{
	using WanderInfo = Enemy::WanderInfo;
	WanderInfo& wander = enemy->GetWanderInfo();

	// 静的な乱数エンジンを1個だけ確保してずっと使い回す
	static thread_local std::mt19937 rng{ std::random_device{}() };

	// 0.0f 〜 1.0f の一様乱数
	std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

	// ランダムな方向(0〜2π)
	float twoPi = 2.0f * std::numbers::pi_v<float>;
	float r01 = dist01(rng);
	wander.targetYaw = r01 * twoPi;

	// ランダムな徘徊持続時間（wanderChangeIntervalMin_〜Max_）
	std::uniform_real_distribution<float> distTime(wander.changeIntervalMin, wander.changeIntervalMax);
	wander.timer = distTime(rng);
}
