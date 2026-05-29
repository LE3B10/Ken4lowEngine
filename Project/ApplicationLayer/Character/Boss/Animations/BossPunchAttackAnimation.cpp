#include "BossPunchAttackAnimation.h"
#include "BossAnimationComponent.h"
#include "BossPunchAttack.h"
#include "BossBase.h"

/// -------------------------------------------------------------
///				　		　 判定処理
/// -------------------------------------------------------------
bool BossPunchAttackAnimation::CanHandle(const IBossAttack* attack) const
{
	// 現在攻撃が BossPunchAttack ならこのクラスが担当する
	return dynamic_cast<const BossPunchAttack*>(attack) != nullptr;
}

/// -------------------------------------------------------------
///				　		アニメ更新処理
/// -------------------------------------------------------------
void BossPunchAttackAnimation::UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime)
{
	// attack はこのアニメーションが担当する攻撃であることが保証されているので、BossPunchAttack にキャストしても安全
	const auto punchAttack = dynamic_cast<const BossPunchAttack*>(attack);
	(void)punchAttack;

	// Punch 用ポーズを組み立てて反映
	auto pose = animationComponent.BuildPunchPose();

	// Punch は振りかぶる感じで、腕を下げる
	animationComponent.ApplyPose(boss, pose, deltaTime);
}