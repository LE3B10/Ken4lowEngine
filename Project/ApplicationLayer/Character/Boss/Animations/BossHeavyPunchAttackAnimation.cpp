#include "BossHeavyPunchAttackAnimation.h"
#include "Components/BossAnimationComponent.h"
#include "Attacks/BossHeavyPunchAttack.h"
#include "Core/BossBase.h"

bool BossHeavyPunchAttackAnimation::CanHandle(const IBossAttack* attack) const
{
	// 現在攻撃が BossHeavyPunchAttack ならこのクラスが担当する
	return dynamic_cast<const BossHeavyPunchAttack*>(attack) != nullptr;
}

void BossHeavyPunchAttackAnimation::UpdatePose(
	BossAnimationComponent& animationComponent,
	BossBase& boss,
	IBossAttack* attack,
	float deltaTime)
{
	(void)attack;

	// HeavyPunch 用ポーズを組み立てて反映
	auto pose = animationComponent.BuildHeavyPunchPose();
	animationComponent.ApplyPose(boss, pose, deltaTime);
}