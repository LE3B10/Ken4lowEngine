#pragma once
#include "Animations/IBossAttackAnimation.h"

class BossPunchAttack;

/// -------------------------------------------------------------
/// Punch 専用攻撃アニメ
/// -------------------------------------------------------------
class BossPunchAttackAnimation : public IBossAttackAnimation
{
public: 

	/// <summary>
	/// このアニメが Punch を扱えるか判定
	/// </summary>
	bool CanHandle(const IBossAttack* attack) const override;

	/// <summary>
	/// Punch の見た目ポーズを更新
	/// 実際のポーズ構築は BossAnimationComponent 側の
	/// BuildPunchPose() を使う
	/// </summary>
	void UpdatePose(
		BossAnimationComponent& animationComponent,
		BossBase& boss,
		IBossAttack* attack,
		float deltaTime) override;
};