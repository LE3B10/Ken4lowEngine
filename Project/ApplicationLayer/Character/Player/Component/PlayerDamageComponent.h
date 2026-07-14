#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>

#include "Segment.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Collider;
	class Camera;
	class CharacterHealthComponent;
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

enum class PlayerHitPart : uint8_t
{
	Body,
};

/// 被弾履歴とPlayer固有フィードバックだけを担当し、HPの正本はCharacterHealthComponentへ委譲する。
class PlayerDamageComponent
{
public:
	struct State
	{
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

	/// 旧呼び出し互換の最大HP設定だけを保持し、現在HPは所有しない。
	void Initialize(float maxHp)
	{
		configuredMaxHp_ = maxHp > 1.0f ? maxHp : 1.0f;
		health_ = nullptr;
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

	float GetHP() const;
	float GetMaxHP() const;
	void Heal(float amount);

	bool IsRecentBulletHit(uint32_t id) const
	{
		return state_.recentBulletHits.find(id) != state_.recentBulletHits.end();
	}

	void MarkRecentBulletHit(uint32_t id);

private:
	K4E::CharacterHealthComponent* BindHealth(Player& player);
	const K4E::CharacterHealthComponent* GetHealth() const { return health_; }

	DamageFeedback ApplyDamageAndHandleDeath(
		Player& player,
		float damage,
		PlayerViewComponent& view,
		PlayerWeaponController& weaponController,
		PlayerDeathComponent& death,
		InputSnapshot& inputSnap,
		bool& runCarry,
		const std::function<void()>& onHitSE,
		const std::function<void()>& onDeathSE);

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
	K4E::CharacterHealthComponent* health_ = nullptr; // 所有権はPlayer Actorが持ち、このComponentは参照だけを保持する。
	float configuredMaxHp_ = 100.0f;
};
