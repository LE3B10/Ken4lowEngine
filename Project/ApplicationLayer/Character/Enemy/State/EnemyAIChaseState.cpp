#define NOMINMAX
#include "EnemyAIChaseState.h"
#include "Enemy.h"
#include "Player.h"
#include <EnemyAIAttackState.h>

namespace K4E = ::Ken4lowEngine;

void EnemyAIChaseState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// 初期状態はChaseに設定
	enemy->SetState(AIState::Chase);

	using FlashInfo = Enemy::FlashInfo;

	// フラッシュ色を元に戻す
	FlashInfo& flash = enemy->GetFlashInfo();
	flash.colorModulate = flash.baseColor;
}

void EnemyAIChaseState::Update(Enemy* enemy, float deltaTime)
{
	(void)deltaTime;

	using AIState = Enemy::AIState;
	using AttackInfo = Enemy::AttackInfo;
	using WanderInfo = Enemy::WanderInfo;
	using BodyPart = Enemy::BodyPart;

	Player* player = enemy->GetPlayer();
	BodyPart& body = enemy->GetBody();
	AttackInfo& attack = enemy->GetAttackInfo();

	// どのステートでもクールダウンは進めたいので、ここで減らす
	if (attack.cooldownTimer > 0.0f) {
		attack.cooldownTimer -= deltaTime;
		if (attack.cooldownTimer < 0.0f) attack.cooldownTimer = 0.0f;
	}

	if (!player) return;

	// プレイヤーが死んでいたら徘徊に戻る
	if (player->IsDeadNow())
	{
		enemy->ResetStateTimer();
		enemy->SetState(AIState::Wander);
		enemy->ChangeState(std::make_unique<EnemyAIWanderState>());
		return;
	}

	K4E::Vector3 playerPos = player->GetCenterPosition();
	K4E::Vector3 enemyPos = body.transform.translate_;

	K4E::Vector3 diff = playerPos - enemyPos; diff.y = 0.0f;
	float  dist = std::max(0.0001f, K4E::Vector3::Length(diff));
	K4E::Vector3 dir = diff / dist;

	const float minDist = enemy->GetPersonalSpaceRadius();
	const float triggerDist = std::max(attack.range, minDist + attack.reachMargin);

	// 常にプレイヤーの方を向く
	body.transform.rotate_.y = std::atan2f(-dir.x, dir.z);

	// 近づくが、minDist を踏み越えない
	if (dist > minDist)
	{
		float step = enemy->GetChaseSpeed();
		if (dist - step < minDist) step = dist - minDist;
		if (step > 0.0f) body.transform.translate_ += dir * step;
	}
	// dist <= minDist のときは下がらない（押されない）

	// 移動「後」の距離で判定し直す
	K4E::Vector3 d2 = player->GetCenterPosition() - body.transform.translate_;
	d2.y = 0.0f;
	float distAfter = K4E::Vector3::Length(d2);

	if (distAfter <= triggerDist && attack.cooldownTimer <= 0.0f)
	{
		// 攻撃状態へ移行
		enemy->ResetStateTimer();
		enemy->SetState(AIState::Attack);
		enemy->ChangeState(std::make_unique<EnemyAIAttackState>());
		attack.didHitThisAttack = false;
		return;
	}
}

void EnemyAIChaseState::Exit(Enemy* enemy)
{
	(void)enemy;
}
