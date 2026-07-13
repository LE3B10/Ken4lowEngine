#pragma once

#include "IBossAttackAnimation.h"

/// 同じ更新経路で通常パンチと強パンチのポーズを切り替える攻撃アニメーション。
class BossPunchAttackAnimation : public IBossAttackAnimation
{
public:
	enum class Type
	{
		Normal,
		Heavy,
	};

	explicit BossPunchAttackAnimation(Type type) : type_(type) {}
	bool CanHandle(const IBossAttack* attack) const override;
	void UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime) override;

private:
	Type type_ = Type::Normal;
};
