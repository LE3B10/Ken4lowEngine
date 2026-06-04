#include "BossStatusComponent.h"

/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void BossStatusComponent::Initialize(float maxHP)
{
	maxHP_ = std::max(1.0f, maxHP);
	hp_ = maxHP_;

	isInvincible_ = false;
	invincibleTimer_ = 0.0f;
	hitStunCooldownTimer_ = 0.0f;
}

/// -------------------------------------------------------------
///						　更新処理
/// -------------------------------------------------------------
void BossStatusComponent::Update(float deltaTime)
{
	// 時間制無敵の更新
	if (invincibleTimer_ > 0.0f)
	{
		// 無敵時間を減らす
		invincibleTimer_ -= deltaTime;

		// 無敵時間が0未満にならないように丸める
		if (invincibleTimer_ < 0.0f) invincibleTimer_ = 0.0f;
	}

	// 怯みクールダウン中でもHP処理は通常通り行い、再怯みだけを遅らせる。
	hitStunCooldownTimer_ = std::max(0.0f, hitStunCooldownTimer_ - deltaTime);
}

/// -------------------------------------------------------------
///						ダメージ適用
/// -------------------------------------------------------------
void BossStatusComponent::ApplyDamage(float damage)
{
	// マイナスダメージは無視
	if (damage <= 0.0f)	return;

	// 常時無敵 または 時間制無敵中なら受けない
	if (isInvincible_ || invincibleTimer_ > 0.0f) return;

	// ダメージを適用
	hp_ -= damage;

	// HPは0未満にならないように丸める
	if (hp_ < 0.0f) hp_ = 0.0f;
}

/// -------------------------------------------------------------
///						回復処理
/// -------------------------------------------------------------
void BossStatusComponent::Heal(float value)
{
	// マイナス回復は無視
	if (value <= 0.0f) return;

	// HPを回復
	hp_ += value;

	// HPは最大HPを超えないように丸める
	if (hp_ > maxHP_) hp_ = maxHP_;
}

/// -------------------------------------------------------------
///						全回復処理
/// -------------------------------------------------------------
void BossStatusComponent::FullRecover()
{
	hp_ = maxHP_;
}

/// -------------------------------------------------------------
///						最大HP設定
/// -------------------------------------------------------------
void BossStatusComponent::SetMaxHP(float maxHP)
{
	maxHP_ = std::max(1.0f, maxHP);

	// 新しい最大値に丸める
	if (hp_ > maxHP_) hp_ = maxHP_;
}

/// -------------------------------------------------------------
///						HP直接設定
/// -------------------------------------------------------------
void BossStatusComponent::SetHP(float hp)
{
	// 0～maxHPの範囲に収める
	hp_ = std::clamp(hp, 0.0f, maxHP_);
}

/// -------------------------------------------------------------
///					無敵フラグ設定
/// -------------------------------------------------------------
void BossStatusComponent::SetInvincible(bool isInvincible)
{
	// 常時無敵フラグをセット
	isInvincible_ = isInvincible;
}

/// -------------------------------------------------------------
///					時間制無敵設定
/// -------------------------------------------------------------
void BossStatusComponent::SetInvincibleTimer(float timeSec)
{
	// 0未満の時間は無効
	invincibleTimer_ = std::max(0.0f, timeSec);
}

/// -------------------------------------------------------------
///						HP割合
/// -------------------------------------------------------------
float BossStatusComponent::GetHPRate() const
{
	// maxHP が 0 以下なら 0 を返す
	if (maxHP_ <= 0.0f)	return 0.0f;

	// HP割合を計算して返す
	return hp_ / maxHP_;
}

/// -------------------------------------------------------------
///					怯みクールダウン可否
/// -------------------------------------------------------------
bool BossStatusComponent::CanStartHitStun() const
{
	return hitStunCooldownTimer_ <= 0.0f;
}

/// -------------------------------------------------------------
///					怯みクールダウン開始
/// -------------------------------------------------------------
void BossStatusComponent::StartHitStunCooldown()
{
	hitStunCooldownTimer_ = hitStunCooldownSec_;
}

/// -------------------------------------------------------------
///					怯みクールダウン秒数設定
/// -------------------------------------------------------------
void BossStatusComponent::SetHitStunCooldownSec(float cooldownSec)
{
	hitStunCooldownSec_ = std::max(0.0f, cooldownSec);
	hitStunCooldownTimer_ = std::min(hitStunCooldownTimer_, hitStunCooldownSec_);
}
