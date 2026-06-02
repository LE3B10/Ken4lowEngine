#pragma once
#include <functional>
#include <unordered_map>
#include <Vector3.h>
#include <Segment.h>

namespace K4E = ::Ken4lowEngine;

class CollisionManager;
class PlayerViewComponent;
class EnemyBase;
class EnemySpawnCrystal;

class PlayerMeleeComponent
{
public: /// ---------- メンバ関数 ---------- ///

	void BindDependencies(PlayerViewComponent* view, CollisionManager* collisionManager);

	void StartAttack(const K4E::Vector3& playerPos);
	void Tick(float dt, const K4E::Vector3& playerPos);

	bool IsActive() const { return isAttacking_; }
	bool IsFinished() const { return !isAttacking_; }

	void SetOnHitCallback(std::function<void()> cb) { onHit_ = std::move(cb); }

private: /// ---------- 補助関数 ---------- ///

	void EvaluateHit(const K4E::Vector3& playerPos);

private: /// ---------- メンバ変数 ---------- ///

	PlayerViewComponent* view_ = nullptr;
	CollisionManager* collisionManager_ = nullptr;

	bool isAttacking_ = false;
	bool activeHitDone_ = false;

	float timer_ = 0.0f;
	float startupSec_ = 0.08f;
	float activeSec_ = 0.10f;
	float recoverySec_ = 0.20f;

	float range_ = 8.0f;  // 斬撃の届く距離
	float radius_ = 0.75f; // 斬撃の当たり判定半径
	int damage_ = 35;

	std::function<void()> onHit_;
};