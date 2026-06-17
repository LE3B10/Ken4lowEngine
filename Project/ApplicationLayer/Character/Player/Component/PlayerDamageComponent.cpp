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

#include <algorithm>
#include <cmath>

namespace
{
	K4E::Vector3 NormalizeXZOrFallback(K4E::Vector3 direction, const K4E::Vector3& fallback)
	{
		direction.y = 0.0f;
		const float lenSq = direction.x * direction.x + direction.z * direction.z;
		if (lenSq <= 0.0001f)
		{
			return fallback;
		}

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
		if (!tr)
		{
			return;
		}

		K4E::Vector3 knockbackDir = MakeCameraBackDirection(view);
		if (attackerPosition)
		{
			// 攻撃元が分かる場合は、攻撃元からプレイヤーを押し出す方向にする。
			knockbackDir = NormalizeXZOrFallback(tr->translate_ - *attackerPosition, knockbackDir);
		}

		const float strength = std::clamp(hitStrength01, 0.10f, 1.00f);
		const float horizontalDistance = 0.45f + strength * 0.85f;
		const float verticalLift = 0.04f + strength * 0.16f;

		// 被弾時に少しだけ即時押し出し、ダメージを受けた手応えを作る。
		tr->translate_.x += knockbackDir.x * horizontalDistance;
		tr->translate_.z += knockbackDir.z * horizontalDistance;
		tr->translate_.y += verticalLift;
		player.SetCenterPosition(tr->translate_);
	}
}

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
	K4E::Vector3 attackerPosition = bullet->GetCenterPosition();
	if (auto* b = bullet->GetOwner<Bullet>())
	{
		baseDmg = b->GetDamage();
		attackerPosition = b->GetShooterPosition();
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
	DamageFeedback fb{};
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;

	if (death.IsActive() || damage <= 0.0f || state_.hp <= 0.0f)
	{
		return fb;
	}

	const bool wasAlive = (state_.hp > 0.0f);
	state_.hp -= damage;
	if (state_.hp < 0.0f)
	{
		state_.hp = 0.0f;
	}

	fb.tookDamage = true;
	fb.hpChanged = true;
	fb.notifyPlayerHit = true;
	fb.damage = damage;
	fb.hpAfter = state_.hp;
	fb.maxHp = state_.maxHp;
	fb.hitStrength01 = CalcHitStrength01(damage);

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
		}

		StartDeath(player, view, weaponController, death, inputSnap, runCarry, launchDir);
		fb.startedDeath = true;
		return fb;
	}

	ApplyDamageKnockback(player, view, nullptr, fb.hitStrength01);

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
