#pragma once
#include "Animations/IBossAttackAnimation.h"

class BossHeavyPunchAttack;

/// -------------------------------------------------------------
/// HeavyPunch 専用攻撃アニメ
/// -------------------------------------------------------------
class BossHeavyPunchAttackAnimation : public IBossAttackAnimation
{
public:
	/// <summary>
	/// このアニメが HeavyPunch を扱えるか判定
	/// </summary>
	bool CanHandle(const IBossAttack* attack) const override;

	/// <summary>
	/// HeavyPunch の見た目ポーズを更新
	/// 実際のポーズ構築は BossAnimationComponent 側の
	/// BuildHeavyPunchPose() を使う
	/// </summary>
	void UpdatePose(
		BossAnimationComponent& animationComponent,
		BossBase& boss,
		IBossAttack* attack,
		float deltaTime) override;
};