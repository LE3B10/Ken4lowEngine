#include "EnemyAIDeadState.h"
#include "Enemy.h"
#include "Vector3.h"

#include <random>

void EnemyAIDeadState::Enter(Enemy* enemy)
{
	using AIState = Enemy::AIState;

	// 初期状態はDeadに設定
	enemy->SetState(AIState::Dead);
}

void EnemyAIDeadState::Update(Enemy* enemy, float deltaTime)
{
	using AIState = Enemy::AIState;

	using DeathEnemyState = Enemy::DeathEnemyState;
	using GibMotion = Enemy::GibMotion;
	using FlashInfo = Enemy::FlashInfo;
	using BodyPart = BaseCharacter::BodyPart;

	BodyPart& body = enemy->GetBody();
	std::vector<BodyPart>& parts = enemy->GetBodyParts();
	FlashInfo& flash = enemy->GetFlashInfo();
	DeathEnemyState& death = enemy->GetDeathState();

	death.timer += deltaTime;
	float t = std::clamp(death.timer / death.duration, 0.0f, 1.0f);
	const Vector3 gravity = { 0.0f, -9.8f * 3.0f, 0.0f };

	// body も飛ばす
	death.bodyGib.velocity += gravity * deltaTime;
	body.transform.translate_ += death.bodyGib.velocity * deltaTime;
	body.transform.rotate_ += death.bodyGib.angularVelocity * deltaTime;

	for (size_t i = 0; i < parts.size() && i < death.gibs.size(); ++i)
	{
		auto& part = parts[i];
		auto& gm = death.gibs[i];

		gm.velocity += gravity * deltaTime;
		part.transform.translate_ += gm.velocity * deltaTime;
		part.transform.rotate_ += gm.angularVelocity * deltaTime;

		// だんだん消えていく（ディゾルブがあるならそこに繋ぐ）
		float alpha = 1.0f - t;
		part.object->SetColor({ flash.colorModulate.x,flash.colorModulate.y,flash.colorModulate.z,alpha });
	}

	if (death.timer >= death.duration)
	{
		death.active = false;
		death.finished = true;
		enemy->SetActive(false);
	}
}

void EnemyAIDeadState::Exit(Enemy* enemy)
{
	(void)enemy;
}
