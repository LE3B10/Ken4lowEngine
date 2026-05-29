#include "BossHeavyPunchAttackAnimation.h"
#include "BossAnimationComponent.h"
#include "BossHeavyPunchAttack.h"
#include "BossBase.h"

/// -------------------------------------------------------------
///				　		　 判定処理
/// -------------------------------------------------------------
bool BossHeavyPunchAttackAnimation::CanHandle(const IBossAttack* attack) const
{
	// 現在攻撃が BossHeavyPunchAttack ならこのクラスが担当する
	return dynamic_cast<const BossHeavyPunchAttack*>(attack) != nullptr;
}

/// -------------------------------------------------------------
///				　		アニメ更新処理
/// -------------------------------------------------------------
void BossHeavyPunchAttackAnimation::UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime)
{
	// attack はこのアニメーションが担当する攻撃であることが保証されているので、BossHeavyPunchAttack にキャストしても安全
	const auto heavyPunchAttack = dynamic_cast<const BossHeavyPunchAttack*>(attack);
	(void)heavyPunchAttack;

	// HeavyPunch 用ポーズを組み立てて反映
	auto pose = animationComponent.BuildHeavyPunchPose();

	// Punch より大きく振りかぶる感じで、腕をさらに下げる
	animationComponent.ApplyPose(boss, pose, deltaTime);
}