#pragma once
#include "PlayerHurtbox.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "Segment.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Collider;
	class Camera;
}
namespace K4E = ::Ken4lowEngine;

class Bullet;
class Player;
class PlayerViewComponent;
class PlayerWeaponController;
class PlayerDeathComponent;
struct InputSnapshot;
struct FallDamageSettings;
struct PlayerAPI;

/// ------------------------------------------------------------
/// PlayerDamageComponent
/// - 被弾 / 最近ヒット弾管理 / 落下ダメージ を担当
/// - HUD や VFX には直接触らず、結果だけ返す
/// ------------------------------------------------------------
class PlayerDamageComponent
{
public:
	struct State
	{
		float maxHp = 100.0f;
		float hp = 100.0f;
		std::unordered_map<uint32_t, float> recentBulletHits{};
		float recentBulletHitTTL = 0.25f;
	};

	struct DamageFeedback
	{
		bool tookDamage = false;
		bool hpChanged = false;
		bool notifyPlayerHit = false;
		bool startedDeath = false;

		float damage = 0.0f;
		float hpAfter = 0.0f;
		float maxHp = 0.0f;
		float hitStrength01 = 0.0f;
	};

	void Initialize(float maxHp)
	{
		state_.maxHp = maxHp;
		state_.hp = maxHp;
	}

	void Tick(float dt);

	DamageFeedback OnHitByEnemyBullet(
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
		std::function<void()> onDeathSE);

	DamageFeedback ApplyFallDamage(
		Player& player,
		float deltaTime,
		const FallDamageSettings& settings,
		PlayerViewComponent& view,
		PlayerWeaponController& weaponController,
		PlayerDeathComponent& death,
		InputSnapshot& inputSnap,
		PlayerAPI& api,
		bool& runCarry,
		std::function<void()> onDeathSE);

	DamageFeedback ApplyDamage(
		Player& player,
		float damage,
		PlayerViewComponent& view,
		PlayerWeaponController& weaponController,
		PlayerDeathComponent& death,
		InputSnapshot& inputSnap,
		bool& runCarry,
		std::function<void()> onHitSE,
		std::function<void()> onDeathSE);

	float GetHP() const { return state_.hp; }
	float GetMaxHP() const { return state_.maxHp; }
	void Heal(float amount);

	bool IsRecentBulletHit(uint32_t id) const
	{
		return state_.recentBulletHits.find(id) != state_.recentBulletHits.end();
	}

	void MarkRecentBulletHit(uint32_t id);

private:
	void StartDeath(
		Player& player,
		PlayerViewComponent& view,
		PlayerWeaponController& weaponController,
		PlayerDeathComponent& death,
		InputSnapshot& inputSnap,
		bool& runCarry,
		const K4E::Vector3& launchDirWorld);

	float CalcHitStrength01(float damage) const;

private:
	State state_{};
};