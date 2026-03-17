#include "BossStatusComponent.h"

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossStatusComponent::Initialize(float maxHP)
{
	maxHP_ = std::max(1.0f, maxHP);
	hp_ = maxHP_;

	isInvincible_ = false;
	invincibleTimer_ = 0.0f;
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossStatusComponent::Update(float deltaTime)
{
	// 時間制無敵の更新
	if (invincibleTimer_ > 0.0f)
	{
		invincibleTimer_ -= deltaTime;
		if (invincibleTimer_ < 0.0f)
		{
			invincibleTimer_ = 0.0f;
		}
	}
}

/// -------------------------------------------------------------
/// ダメージ適用
/// -------------------------------------------------------------
void BossStatusComponent::ApplyDamage(float damage)
{
	// マイナスダメージは無視
	if (damage <= 0.0f)
	{
		return;
	}

	// 常時無敵 または 時間制無敵中なら受けない
	if (isInvincible_ || invincibleTimer_ > 0.0f)
	{
		return;
	}

	hp_ -= damage;
	if (hp_ < 0.0f)
	{
		hp_ = 0.0f;
	}
}

/// -------------------------------------------------------------
/// 回復
/// -------------------------------------------------------------
void BossStatusComponent::Heal(float value)
{
	if (value <= 0.0f)
	{
		return;
	}

	hp_ += value;
	if (hp_ > maxHP_)
	{
		hp_ = maxHP_;
	}
}

/// -------------------------------------------------------------
/// 全回復
/// -------------------------------------------------------------
void BossStatusComponent::FullRecover()
{
	hp_ = maxHP_;
}

/// -------------------------------------------------------------
/// 最大HP設定
/// -------------------------------------------------------------
void BossStatusComponent::SetMaxHP(float maxHP)
{
	maxHP_ = std::max(1.0f, maxHP);

	// 新しい最大値に丸める
	if (hp_ > maxHP_)
	{
		hp_ = maxHP_;
	}
}

/// -------------------------------------------------------------
/// HP直接設定
/// -------------------------------------------------------------
void BossStatusComponent::SetHP(float hp)
{
	hp_ = std::clamp(hp, 0.0f, maxHP_);
}

/// -------------------------------------------------------------
/// 無敵フラグ設定
/// -------------------------------------------------------------
void BossStatusComponent::SetInvincible(bool isInvincible)
{
	isInvincible_ = isInvincible;
}

/// -------------------------------------------------------------
/// 時間制無敵設定
/// -------------------------------------------------------------
void BossStatusComponent::SetInvincibleTimer(float timeSec)
{
	invincibleTimer_ = std::max(0.0f, timeSec);
}

/// -------------------------------------------------------------
/// HP割合
/// -------------------------------------------------------------
float BossStatusComponent::GetHPRate() const
{
	if (maxHP_ <= 0.0f)
	{
		return 0.0f;
	}

	return hp_ / maxHP_;
}