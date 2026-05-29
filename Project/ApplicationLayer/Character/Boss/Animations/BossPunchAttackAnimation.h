#pragma once
#include "IBossAttackAnimation.h"

/// ---------- 前方宣言 ---------- ///
class BossPunchAttack;

/// -------------------------------------------------------------
///				Punch 専用攻撃アニメーション
/// -------------------------------------------------------------
class BossPunchAttackAnimation : public IBossAttackAnimation
{
public: /// ---------- 基本構造 ---------- ///

	// このアニメーションが Punch を扱えるか判定
	bool CanHandle(const IBossAttack* attack) const override;

	// Punch の見た目ポーズを更新
	void UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime) override;
};