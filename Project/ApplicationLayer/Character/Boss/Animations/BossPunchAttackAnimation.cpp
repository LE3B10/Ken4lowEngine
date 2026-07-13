#include "BossPunchAttackAnimation.h"

#include "BossAnimationComponent.h"
#include "BossBase.h"
#include "BossHeavyPunchAttack.h"
#include "BossPunchAttack.h"

bool BossPunchAttackAnimation::CanHandle(const IBossAttack* attack) const
{
	if (type_ == Type::Heavy)
	{
		return dynamic_cast<const BossHeavyPunchAttack*>(attack) != nullptr;
	}
	return dynamic_cast<const BossPunchAttack*>(attack) != nullptr;
}

void BossPunchAttackAnimation::UpdatePose(
	BossAnimationComponent& animationComponent,
	BossBase& boss,
	IBossAttack* attack,
	float deltaTime)
{
	if (!CanHandle(attack)) { return; }

	// 攻撃タイプによる差はポーズ生成だけに限定し、検証と反映の手順は一つにまとめる。
	const auto pose = type_ == Type::Heavy
		? animationComponent.BuildHeavyPunchPose()
		: animationComponent.BuildPunchPose();
	animationComponent.ApplyPose(boss, pose, deltaTime);
}
