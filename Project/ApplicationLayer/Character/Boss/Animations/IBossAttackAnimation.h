#pragma once

/// ---------- 前方宣言 ---------- ///
class BossBase;
class IBossAttack;
class BossAnimationComponent;

/// -------------------------------------------------------------
/// 攻撃アニメーションの共通インターフェース
///
/// 役割:
/// - 攻撃種別ごとの見た目ポーズ構築を担当する
/// - BossAnimationComponent から共通の形で呼べるようにする
/// -------------------------------------------------------------
class IBossAttackAnimation
{
public:
	virtual ~IBossAttackAnimation() = default;

	/// <summary>
	/// このアニメクラスが対象攻撃を扱えるか
	/// </summary>
	virtual bool CanHandle(const IBossAttack* attack) const = 0;

	/// <summary>
	/// 攻撃ポーズを更新する
	/// 実際の ApplyPose は BossAnimationComponent 側に任せる
	/// </summary>
	virtual void UpdatePose(
		BossAnimationComponent& animationComponent,
		BossBase& boss,
		IBossAttack* attack,
		float deltaTime) = 0;
};