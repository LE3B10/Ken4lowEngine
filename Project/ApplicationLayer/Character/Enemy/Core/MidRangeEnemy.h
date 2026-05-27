#pragma once

#include "EnemyBase.h"
#include "../Navigation/EnemyAStarNavigator.h"
#include <filesystem>
#include <string>

class BulletManager;

namespace K4E = ::Ken4lowEngine;

class MidRangeEnemy final : public EnemyBase
{
private:
	// 追加: 中距離敵の行動状態
	enum class ActionState
	{
		ChaseTargetAction,
		KeepDistanceAction,
		RetreatAction,
		MidRangeAttackAction,
		CombatIdleAction,
		DeadAction,
	};

	struct MidRangeDistanceSettings
	{
		float detectRange = 22.0f;
		float attackMinRange = 5.0f;
		float attackMaxRange = 12.0f;
		float keepDistance = 8.0f;
		float tooCloseDistance = 4.0f;
		float resumeChaseDistance = 13.0f;
	};

	struct MidRangeMoveSettings
	{
		float moveSpeed = 3.0f;
		float retreatSpeed = 2.6f;
		float rotateSpeed = 7.0f;
	};

	struct MidRangeAttackSettings
	{
		float cooldown = 1.5f;
		float castTime = 0.35f;
		float projectileSpeed = 12.0f;
		float projectileLifeTime = 3.0f;
		int damage = 10;
		float attackRadius = 0.6f;
	};

	struct HeadLookSettings
	{
		float headYawFollowSpeed = 8.0f;
		float maxHeadYaw = 0.8f;
	};

	struct TuningIo
	{
		std::filesystem::path jsonPath = "Resources/DataAssets/Enemy/MidRangeEnemy/MidRangeEnemy_Normal.json";
		std::string lastLoadResult = "未読み込み";
		std::string lastSaveResult = "未保存";
	};

public:
	void Initialize() override;
	void Update(float deltaTime) override;
	void DrawImGui() override;
	void TakeDamage(int amount) override;

	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetBulletManager(BulletManager* bm) { bulletManager_ = bm; }
	const char* GetCurrentBehaviorName() const;

private:
	void UpdateAction(float deltaTime);
	void UpdateMoveAndFacing(float deltaTime, const K4E::Vector3& toTarget, float distance);
	void UpdateAttack(float deltaTime, const K4E::Vector3& toTarget, float distance);
	void FireProjectile(const K4E::Vector3& toTarget);
	void UpdateAnimation(float deltaTime, bool moving, bool casting);
	void UpdateHeadLook(float deltaTime, const K4E::Vector3& toTarget);
	bool LoadTuningFromJson(const std::filesystem::path& path, std::string* outMessage);
	bool SaveTuningToJson(const std::filesystem::path& path, std::string* outMessage) const;

private:
	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	EnemyAStarNavigator navigator_{};
	ActionState actionState_ = ActionState::CombatIdleAction;
	MidRangeDistanceSettings distanceSettings_{};
	MidRangeMoveSettings moveSettings_{};
	MidRangeAttackSettings attackSettings_{};
	HeadLookSettings headLookSettings_{};
	TuningIo tuningIo_{};
	float cooldownTimer_ = 0.0f;
	float castTimer_ = 0.0f;
	float yaw_ = 0.0f;
	float headYaw_ = 0.0f;
};
