#pragma once

#include <Vector3.h>
#include <Vector4.h>
#include <string>
#include <vector>
#include <AABB.h>

namespace K4E = ::Ken4lowEngine;


struct BombProjectileSettings
{
	float initialSpeed = 10.0f;
	float upwardVelocity = 7.0f;
	float gravity = 18.0f;
	float lifeTime = 4.0f;
	float explosionRadius = 3.0f;
	int directHitDamage = 25;
	int explosionDamage = 12;
	float hitRadius = 0.45f;
	bool directHitAlsoExplosionDamage = false;
};

class MidRangeBombProjectile
{
public:
	// 修正: Collider依存を外すためLaunchのowner引数を削除。
	void Launch(
		const K4E::Vector3& startPosition,
		const K4E::Vector3& targetPosition,
		const BombProjectileSettings& settings);

	void Update(float deltaTime, const std::vector<K4E::AABB>* floorAabbs, const std::vector<K4E::AABB>* obstacleAabbs);
	void DrawDebug() const;
	bool IsAlive() const;
	bool IsExploded() const;
	const char* GetDebugLastReason() const;
	const K4E::Vector3& GetPosition() const;
	float GetExplosionRadius() const;
	bool ConsumeDirectHitEvent();
	bool ConsumeExplosionEvent();

private:
	void Explode(const char* reason);
	bool CheckAabbHit(const std::vector<K4E::AABB>* aabbs) const;

private:
	K4E::Vector3 position_{};
	K4E::Vector3 velocity_{};
	float lifeTimer_ = 0.0f;
	bool exploded_ = false;
	bool alive_ = false;
	std::string debugLastReason_ = "未発射";
	BombProjectileSettings settings_{};
	bool directHitEvent_ = false;
	bool explosionEvent_ = false;
};
