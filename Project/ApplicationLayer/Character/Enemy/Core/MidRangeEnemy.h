#pragma once

#include "EnemyBase.h"
#include "MidRangeBombProjectile.h"
#include <filesystem>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

class MidRangeEnemy final : public EnemyBase
{
private:
	enum class ActionState
	{
		Chase,
		KeepDistance,
		Retreat,
		CastBomb,
		Idle,
		Dead,
	};

	struct MidRangeDistanceSettings
	{
		float detectRange = 22.0f;
	};

	struct MidRangeMoveSettings
	{
		float moveSpeed = 3.0f;
		float retreatSpeed = 2.6f;
	};

	struct BombAttackSettings
	{
		float attackMinRange = 5.0f;
		float attackMaxRange = 14.0f;
		float idealRange = 9.0f;
		float tooCloseRange = 4.0f;
		float cooldown = 2.0f;
		float castTime = 0.45f;
		float throwHeightOffset = 1.4f;
	};

public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void DrawImGui() override;
	void TakeDamage(int amount) override;

	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetFloorAABBs(const std::vector<K4E::AABB>* aabbs) { floorAABBs_ = aabbs; }
	void SetWallObstacleAABBs(const std::vector<K4E::AABB>* aabbs) { wallObstacleAABBs_ = aabbs; }

private:
	void UpdateAction(float deltaTime);
	void UpdateAttack(float deltaTime, const K4E::Vector3& toTarget, float distance);
	void ThrowBomb(const K4E::Vector3& toTarget);
	bool LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage);
	bool SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage) const;

private:
	K4E::Collider* target_ = nullptr;
	ActionState actionState_ = ActionState::Idle;
	MidRangeDistanceSettings distanceSettings_{};
	MidRangeMoveSettings moveSettings_{};
	BombAttackSettings bombAttackSettings_{};
	BombProjectileSettings bombProjectileSettings_{};
	MidRangeBombProjectile activeBomb_{};
	const std::vector<K4E::AABB>* floorAABBs_ = nullptr;
	const std::vector<K4E::AABB>* wallObstacleAABBs_ = nullptr;
	float cooldownTimer_ = 0.0f;
	float castTimer_ = 0.0f;
	bool castingBomb_ = false;
	std::string lastThrowReason_ = "未投擲";
	std::filesystem::path jsonPath_ = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
};
