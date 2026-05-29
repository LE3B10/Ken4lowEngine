#pragma once
#include "IBossAttackAnimation.h"

/// ---------- 前方宣言 ---------- ///
class BossHeavyPunchAttack;

/// -------------------------------------------------------------
/// HeavyPunch 専用攻撃アニメ
/// -------------------------------------------------------------
class BossHeavyPunchAttackAnimation : public IBossAttackAnimation
{
public: /// ---------- 基本構造 ---------- ///

	// このアニメーションが HeavyPunch を扱えるか判定
	bool CanHandle(const IBossAttack* attack) const override;

	// HeavyPunch の見た目ポーズを更新
	void UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime) override;
};