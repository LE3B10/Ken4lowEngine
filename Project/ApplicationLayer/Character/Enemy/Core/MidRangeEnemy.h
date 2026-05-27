#pragma once
#include "EnemyBase.h"
#include "ApplicationLayer/Character/Enemy/Projectile/MidRangeBombProjectile.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>


class MidRangeEnemy final : public EnemyBase
{
private: /// ---------- 構造体 ---------- ///

	struct MidRangeDistanceSettings
	{
		float detectRange = 24.0f;
		float attackMinRange = 5.0f;
		float attackMaxRange = 14.0f;
		float idealRange = 9.0f;
		float tooCloseRange = 4.0f;
	};
	struct MidRangeMoveSettings
	{
		float moveSpeed = 2.6f;
		float retreatSpeed = 2.8f;
		float rotateSpeed = 8.0f;
	};
	struct BombAttackSettings
	{
		float cooldown = 2.0f;
		float castTime = 0.45f;
		float throwHeightOffset = 1.6f;
	};
	struct BombAttackState
	{
		float cooldownTimer = 0.0f;
		float castTimer = 0.0f;
		bool casting = false;
		std::string lastReason = "None";
	};

public:

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;
	void DrawImGui() override;

public: /// ---------- アクセッサ ---------- ///

	void SetTarget(const Ken4lowEngine::Vector3& target) { targetPosition_ = target; hasTarget_ = true; }

private:
	
	bool HasTarget() const { return hasTarget_; }
	
	void SaveToJson(const std::filesystem::path& path) const;
	
	void LoadFromJson(const std::filesystem::path& path);

private:
	
	MidRangeDistanceSettings distanceSettings_{};
	MidRangeMoveSettings moveSettings_{};
	BombAttackSettings attackSettings_{};
	BombProjectileSettings projectileSettings_{};
	BombAttackState attackState_{};
	Ken4lowEngine::Vector3 targetPosition_{};
	bool hasTarget_ = false;
	std::vector<std::unique_ptr<MidRangeBombProjectile>> bombs_{};
	std::filesystem::path jsonPath_ = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
};
