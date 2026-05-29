#pragma once

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;
class BossAnimationComponent;

/// -------------------------------------------------------------
///			 攻撃アニメーションの共通インターフェース
/// -------------------------------------------------------------
class IBossAttackAnimation
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// デストラクタは仮想関数にしておく
	virtual ~IBossAttackAnimation() = default;

	// このアニメがこの攻撃を扱えるか判定
	virtual bool CanHandle(const IBossAttack* attack) const = 0;

	// 攻撃の見た目ポーズを更新
	virtual void UpdatePose(BossAnimationComponent& animationComponent, BossBase& boss, IBossAttack* attack, float deltaTime) = 0;
};