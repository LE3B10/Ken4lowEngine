#pragma once

#include <cstdint>
#include <Vector3.h>

namespace K4E = Ken4lowEngine;

class BossBase;

struct BossMeleeHitSettings
{
	float range = 0.0f;
	float radius = 0.0f;
	float forwardOffset = 0.0f;
	float angleDeg = 0.0f;
	float targetRadius = 0.0f;
	float heightOffset = 0.0f;
};

struct BossImpactParticleSettings
{
	uint32_t spawnCount = 0;
	float spawnRadius = 0.0f;
	float lifetimeScale = 1.0f;
	float initialSpeedScale = 1.0f;
};

/// ボス近接攻撃で共通する射程、方向固定、命中形状の計算を提供する。
namespace BossMeleeAttackUtility
{
	bool IsTargetInRange(const BossBase* owner, float minRange, float maxRange);
	bool LockDirection(BossBase* owner, K4E::Vector3& lockedForward);
	bool TryCalculateHitCenter(const BossBase& owner, const K4E::Vector3& forward, const BossMeleeHitSettings& settings, K4E::Vector3& hitCenter);
	void NormalizeValidRange(float& minRange, float& maxRange);
	void NormalizeHitSettings(BossMeleeHitSettings& settings);
	void NormalizeParticleSettings(BossImpactParticleSettings& settings);
}
