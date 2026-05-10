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
#include <Vector3.h>


void PlayerDamageComponent::Heal(float amount)
{
	if (amount <= 0.0f) return;
	state_.hp += amount;
	if (state_.hp > state_.maxHp)
	{
		state_.hp = state_.maxHp;
	}
}

void PlayerDamageComponent::Tick(float dt)
{
	for (auto it = state_.recentBulletHits.begin(); it != state_.recentBulletHits.end(); )
	{
		it->second -= dt;
		if (it->second <= 0.0f)
		{
			it = state_.recentBulletHits.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void PlayerDamageComponent::MarkRecentBulletHit(uint32_t id)
{
	if (state_.recentBulletHits.size() > 256)
	{
		state_.recentBulletHits.clear();
	}
	state_.recentBulletHits[id] = state_.recentBulletHitTTL;
}

float PlayerDamageComponent::CalcHitStrength01(float damage) const
{
	float strength01 = (state_.maxHp > 0.0f) ? (damage / state_.maxHp) : 1.0f;
	if (strength01 < 0.10f) strength01 = 0.10f;
	if (strength01 > 1.00f) strength01 = 1.00f;
	return strength01;
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
	DamageFeedback fb{};
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;

	if (!bullet) return fb;

	const uint32_t otherType = bullet->GetTypeID();
	const uint32_t kEnemyBullet = static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet);
	const uint32_t kBossBullet = static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet);

	if (otherType != kEnemyBullet && otherType != kBossBullet)
	{
		return fb;
	}

	const uint32_t bulletId = bullet->GetUniqueID();
	if (IsRecentBulletHit(bulletId))
	{
		return fb;
	}
	MarkRecentBulletHit(bulletId);

	int baseDmg = 1;
	if (auto* b = bullet->GetOwner<Bullet>())
	{
		baseDmg = b->GetDamage();
	}

	const float dmg = static_cast<float>(baseDmg) * mul;
	const bool wasAlive = (state_.hp > 0.0f);

	state_.hp -= dmg;
	if (state_.hp < 0.0f)
	{
		state_.hp = 0.0f;
	}

	fb.tookDamage = true;
	fb.hpChanged = true;
	fb.notifyPlayerHit = true;
	fb.damage = dmg;
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;
	fb.hitStrength01 = CalcHitStrength01(dmg);

	if (onHitSE)
	{
		onHitSE();
	}

	if (wasAlive && state_.hp <= 0.0f)
	{
		if (onDeathSE)
		{
			onDeathSE();
		}

		K4E::Vector3 launchDir = { 0.0f, 0.0f, -1.0f };

		if (auto* cam = view.GetCamera())
		{
			launchDir = -cam->GetForward();
			launchDir.y = 0.0f;

			if (K4E::Vector3::Length(launchDir) > 0.0001f)
			{
				launchDir = K4E::Vector3::Normalize(launchDir);
			}
			else
			{
				launchDir = { 0.0f, 0.0f, -1.0f };
			}
		}

		StartDeath(player, view, weaponController, death, inputSnap, runCarry, launchDir);
		fb.startedDeath = true;
		return fb;
	}

	float stunSec = 0.08f;
	switch (part)
	{
	case PlayerHitPart::Head:
		stunSec = 0.15f;
		break;
	default:
		break;
	}

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

	DamageFeedback fb{};
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;

	if (death.IsActive()) return fb;
	if (!settings.enabled) return fb;

	auto* tr = player.GetWorldTransform();
	if (!tr) return fb;

	if (tr->translate_.y > settings.startY) return fb;

	const bool wasAlive = (state_.hp > 0.0f);

	const float dmg = settings.damagePerSecond * deltaTime;
	state_.hp -= dmg;
	if (state_.hp < 0.0f)
	{
		state_.hp = 0.0f;
	}

	fb.tookDamage = true;
	fb.hpChanged = true;
	fb.notifyPlayerHit = true;
	fb.damage = dmg;
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;
	fb.hitStrength01 = CalcHitStrength01(dmg);

	if (wasAlive && state_.hp <= 0.0f)
	{
		if (onDeathSE)
		{
			onDeathSE();
		}

		K4E::Vector3 launchDir = { 0.0f, 0.0f, -1.0f };
		if (auto* cam = view.GetCamera())
		{
			launchDir = -cam->GetForward();
			launchDir.y = 0.0f;

			if (K4E::Vector3::Length(launchDir) > 0.0001f)
			{
				launchDir = K4E::Vector3::Normalize(launchDir);
			}
		}

		StartDeath(player, view, weaponController, death, inputSnap, runCarry, launchDir);
		fb.startedDeath = true;
	}

	return fb;
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