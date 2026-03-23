#include "BossPunchAttackAnimation.h"
#include "Components/BossAnimationComponent.h"
#include "Attacks/BossPunchAttack.h"
#include "Core/BossBase.h"

bool BossPunchAttackAnimation::CanHandle(const IBossAttack* attack) const
{
	return dynamic_cast<const BossPunchAttack*>(attack) != nullptr;
}

void BossPunchAttackAnimation::UpdatePose(
	BossAnimationComponent& animationComponent,
	BossBase& boss,
	IBossAttack* attack,
	float deltaTime)
{
	(void)attack;

	// Punch 用ポーズを組み立てて反映
	auto pose = animationComponent.BuildPunchPose();
	animationComponent.ApplyPose(boss, pose, deltaTime);
}