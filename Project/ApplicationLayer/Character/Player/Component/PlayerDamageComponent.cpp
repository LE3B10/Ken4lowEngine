#define NOMINMAX
#include "PlayerDamageComponent.h"

#include "Player.h"
#include "PlayerDeathComponent.h"
#include "PlayerViewComponent.h"
#include "PlayerWeaponController.h"
#include "PlayerStateMachines.h"

#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <Collider.h>
#include <Camera.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Vector3.h>

#include <algorithm>
#include <cmath>

namespace
{
	K4E::Vector3 NormalizeXZOrFallback(K4E::Vector3 direction, const K4E::Vector3& fallback)
	{
		direction.y = 0.0f;
		const float lenSq = direction.x * direction.x + direction.z * direction.z;
		if (lenSq <= 0.0001f) return fallback;
		const float invLen = 1.0f / std::sqrt(lenSq);
		return { direction.x * invLen, 0.0f, direction.z * invLen };
	}

	K4E::Vector3 MakeCameraBackDirection(PlayerViewComponent& view)
	{
		if (auto* cam = view.GetCamera())
		{
			K4E::Vector3 direction = -cam->GetForward();
			direction.y = 0.0f;
			return NormalizeXZOrFallback(direction, { 0.0f, 0.0f, -1.0f });
		}
		return { 0.0f, 0.0f, -1.0f };
	}

	void ApplyDamageKnockback(Player& player, PlayerViewComponent& view, const K4E::Vector3* attackerPosition, float hitStrength01)
	{
		auto* tr = player.GetWorldTransform();
		if (!tr) return;

		K4E::Vector3 knockbackDir = MakeCameraBackDirection(view);
		if (attackerPosition) knockbackDir = NormalizeXZOrFallback(tr->translate_ - *attackerPosition, knockbackDir);

		const float strength = std::clamp(hitStrength01, 0.10f, 1.00f);
		const float horizontalDistance = 0.45f + strength * 0.85f;
		const float verticalLift = 0.04f + strength * 0.16f;
		tr->translate_.x += knockbackDir.x * horizontalDistance;
		tr->translate_.z += knockbackDir.z * horizontalDistance;
		tr->translate_.y += verticalLift;
		player.SetCenterPosition(tr->translate_); // 位置変更はActor Rootと共通Colliderへ同じ経路で同期する。
	}
}

K4E::CharacterHealthComponent* PlayerDamageComponent::BindHealth(Player& player)
{
	health_ = player.GetHealthComponent();
	if (!health_) return nullptr;
	health_->SetMaxHealth(configuredMaxHp_); // Player固有の初期最大HPだけを共通Healthへ反映する。
	return health_;
}

float PlayerDamageComponent::GetHP() const
{
	return health_ ? health_->GetCurrentHealth() : configuredMaxHp_; // 初回ダメージ前は共通Healthの既定値と同じ満タン表示を返す。
}

float PlayerDamageComponent::GetMaxHP() const
{
	return health_ ? health_->GetMaxHealth() : configuredMaxHp_;
}

void PlayerDamageComponent::Heal(float amount)
{
	if (amount <= 0.0f || !health_) return;
	health_->Heal(amount);
}

void PlayerDamageComponent::Tick(float dt)
{
	for (auto it = state_.recentBulletHits.begin(); it != state_.recentBulletHits.end(); )
	{
		it->second -= dt;
		if (it->second <= 0.0f) it = state_.recentBulletHits.erase(it);
		else ++it;
	}
}

void PlayerDamageComponent::MarkRecentBulletHit(uint32_t id)
{
	if (state_.recentBulletHits.size() > 256) state_.recentBulletHits.clear();
	state_.recentBulletHits[id] = state_.recentBulletHitTTL;
}

float PlayerDamageComponent::CalcHitStrength01(float damage) const
{
	const float maxHp = GetMaxHP();
	float strength01 = maxHp > 0.0f ? damage / maxHp : 1.0f;
	return std::clamp(strength01, 0.10f, 1.00f);
}

PlayerDamageComponent::DamageFeedback PlayerDamageComponent::OnHitByEnemyBullet(
	Player& player,
	K4E::Collider* bullet,
	PlayerHitPart part,
	float mul,
	PlayerViewComponent& view,
	PlayerWeaponController& weaponController,
	PlayerDeathComponent& death,
	InputSnapshot& inputSnap,
	PlayerAPI& api,
	bool& runCarry,
	std::function<void()> onHitSE,
	std::function<void()> onDeathSE)
{
	BindHealth(player);
	DamageFeedback fb{};
	fb.hpAfter = GetHP();
	fb.maxHp = GetMaxHP();
	if (!bullet) return fb;

	const uint32_t otherType = bullet->GetTypeID();
	const uint32_t kEnemyBullet = static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet);
	const uint32_t kBossBullet = static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet);
	if (otherType != kEnemyBullet && otherType != kBossBullet) return fb;

	const uint32_t bulletId = bullet->GetUniqueID();
	if (IsRecentBulletHit(bulletId)) return fb;
	MarkRecentBulletHit(bulletId);

	int baseDmg = 1;
	K4E::Vector3 attackerPosition = bullet->GetCenterPosition();
	if (auto* b = bullet->GetOwner<Bullet>())
	{
		baseDmg = b->GetDamage();
		attackerPosition = b->GetShooterPosition();
	}

	const float dmg = static_cast<float>(baseDmg) * mul;
	fb = ApplyDamageAndHandleDeath(player, dmg, view, weaponController, death, inputSnap, runCarry, onHitSE, onDeathSE);
	if (fb.startedDeath) return fb;

	ApplyDamageKnockback(player, view, &attackerPosition, fb.hitStrength01);
	(void)part;
	const float stunSec = 0.08f;
	PlayerContext ctx{ api, inputSnap, 0.0f };
	api.player->GetBrainComponent().GetBrain().status.RequestStun(ctx, stunSec);
	api.player->GetBrainComponent().GetBrain().loco.Change(ctx, LocoId::Idle);
	api.player->GetBrainComponent().GetBrain().combat.Change(ctx, CombatId::Hip);
	return fb;
}

PlayerDamageComponent::DamageFeedback PlayerDamageComponent::ApplyFallDamage(
	Player& player,
	float deltaTime,
	const FallDamageSettings& settings,
	PlayerViewComponent& view,
	PlayerWeaponController& weaponController,
	PlayerDeathComponent& death,
	InputSnapshot& inputSnap,
	PlayerAPI& api,
	bool& runCarry,
	std::function<void()> onDeathSE)
{
	(void)api;
	BindHealth(player);
	DamageFeedback fb{};
	fb.hpAfter = GetHP();
	fb.maxHp = GetMaxHP();
	if (death.IsActive() || !settings.enabled) return fb;

	auto* tr = player.GetWorldTransform();
	if (!tr || tr->translate_.y > settings.startY) return fb;
	const float dmg = settings.damagePerSecond * deltaTime;
	return ApplyDamageAndHandleDeath(player, dmg, view, weaponController, death, inputSnap, runCarry, {}, onDeathSE);
}

PlayerDamageComponent::DamageFeedback PlayerDamageComponent::ApplyDamage(
	Player& player,
	float damage,
	PlayerViewComponent& view,
	PlayerWeaponController& weaponController,
	PlayerDeathComponent& death,
	InputSnapshot& inputSnap,
	bool& runCarry,
	std::function<void()> onHitSE,
	std::function<void()> onDeathSE)
{
	BindHealth(player);
	DamageFeedback fb{};
	fb.hpAfter = GetHP();
	fb.maxHp = GetMaxHP();
	if (death.IsActive() || damage <= 0.0f || !health_ || health_->IsDead()) return fb;

	fb = ApplyDamageAndHandleDeath(player, damage, view, weaponController, death, inputSnap, runCarry, onHitSE, onDeathSE);
	if (!fb.startedDeath) ApplyDamageKnockback(player, view, nullptr, fb.hitStrength01);
	return fb;
}

PlayerDamageComponent::DamageFeedback PlayerDamageComponent::ApplyDamageAndHandleDeath(
	Player& player,
	float damage,
	PlayerViewComponent& view,
	PlayerWeaponController& weaponController,
	PlayerDeathComponent& death,
	InputSnapshot& inputSnap,
	bool& runCarry,
	const std::function<void()>& onHitSE,
	const std::function<void()>& onDeathSE)
{
	BindHealth(player);
	DamageFeedback feedback{};
	feedback.hpAfter = GetHP();
	feedback.maxHp = GetMaxHP();
	if (death.IsActive() || damage <= 0.0f || !health_ || health_->IsDead()) return feedback;

	K4E::CharacterDamageInfo damageInfo{};
	damageInfo.amount = damage;
	const K4E::CharacterDamageResult result = health_->ApplyDamage(damageInfo);
	feedback.tookDamage = result.accepted;
	feedback.hpChanged = result.appliedDamage > 0.0f;
	feedback.notifyPlayerHit = result.appliedDamage > 0.0f;
	feedback.damage = result.appliedDamage;
	feedback.hpAfter = health_->GetCurrentHealth();
	feedback.maxHp = health_->GetMaxHealth();
	feedback.hitStrength01 = CalcHitStrength01(result.appliedDamage);
	if (feedback.tookDamage && onHitSE) onHitSE();

	if (result.killed)
	{
		if (onDeathSE) onDeathSE();
		StartDeath(player, view, weaponController, death, inputSnap, runCarry, MakeCameraBackDirection(view));
		feedback.startedDeath = true;
	}
	return feedback;
}

void PlayerDamageComponent::StartDeath(
	Player& player,
	PlayerViewComponent& view,
	PlayerWeaponController& weaponController,
	PlayerDeathComponent& death,
	InputSnapshot& inputSnap,
	bool& runCarry,
	const K4E::Vector3& launchDirWorld)
{
	death.Start(
		player,
		launchDirWorld,
		view,
		player.GetWeaponVisualComponent(),
		inputSnap,
		runCarry,
		[&weaponController]() { weaponController.CancelReloadOnly(); });
}
