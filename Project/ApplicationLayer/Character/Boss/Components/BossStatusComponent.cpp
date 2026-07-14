#include "BossStatusComponent.h"

#include <Scene/Actor/Character/CharacterHealthComponent.h>

#include <algorithm>

using namespace Ken4lowEngine;

void BossStatusComponent::Initialize(CharacterHealthComponent* health, float maxHP)
{
	health_ = health;
	invincibleTimer_ = 0.0f;
	if (health_) health_->ResetHealth(std::max(1.0f, maxHP)); // HPの正本は共通Healthだけに置く。
}

void BossStatusComponent::Update(float deltaTime)
{
	if (invincibleTimer_ <= 0.0f) return;
	invincibleTimer_ = std::max(0.0f, invincibleTimer_ - deltaTime);
	if (invincibleTimer_ <= 0.0f && health_) health_->SetInvulnerable(false);
}

void BossStatusComponent::Finalize()
{
	if (health_) health_->SetInvulnerable(false);
	health_ = nullptr;
	invincibleTimer_ = 0.0f;
}

void BossStatusComponent::ApplyDamage(float damage)
{
	if (!health_ || damage <= 0.0f || IsInvincible()) return;
	CharacterDamageInfo damageInfo{};
	damageInfo.amount = damage;
	health_->ApplyDamage(damageInfo);
}

void BossStatusComponent::Heal(float value)
{
	if (health_ && value > 0.0f) health_->Heal(value);
}

void BossStatusComponent::FullRecover()
{
	if (health_) health_->RestoreFullHealth();
}

void BossStatusComponent::SetMaxHP(float maxHP)
{
	if (health_) health_->SetMaxHealth(std::max(1.0f, maxHP));
}

void BossStatusComponent::SetHP(float hp)
{
	if (health_) health_->SetCurrentHealth(hp);
}

void BossStatusComponent::SetInvincible(bool isInvincible)
{
	if (health_) health_->SetInvulnerable(isInvincible);
	if (!isInvincible) invincibleTimer_ = 0.0f;
}

void BossStatusComponent::SetInvincibleTimer(float timeSec)
{
	invincibleTimer_ = std::max(0.0f, timeSec);
	if (health_) health_->SetInvulnerable(invincibleTimer_ > 0.0f);
}

bool BossStatusComponent::IsInvincible() const
{
	return health_ && health_->IsInvulnerable();
}

float BossStatusComponent::GetHP() const
{
	return health_ ? health_->GetCurrentHealth() : 0.0f;
}

float BossStatusComponent::GetMaxHP() const
{
	return health_ ? health_->GetMaxHealth() : 0.0f;
}

float BossStatusComponent::GetHPRate() const
{
	return health_ ? health_->GetHealthRatio() : 0.0f;
}

bool BossStatusComponent::IsDead() const
{
	return !health_ || health_->IsDead();
}
