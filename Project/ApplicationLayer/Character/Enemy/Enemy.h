#pragma once

#include <string>

#include "EnemyBase.h"
#include "EnemyAICommand.h"
#include "EnemyStateMachine.h"

// 前方宣言
class BulletManager;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// Enemy
///  - EnemyBase の物理（位置/速度）に、意思決定(FSM)と射撃を追加
///  - FSMは「命令（EnemyAICommand）」だけを出し、Enemyが実行する
/// -------------------------------------------------------------
class Enemy final : public EnemyBase
{
public:
	Enemy() = default;
	~Enemy() override = default;

	// 互換用: 旧シーンが Initialize() を呼んでも動くようにする
	void Initialize() { Initialize({ 0.0f, 0.0f, 30.0f }, "cube.gltf"); }

	void Initialize(const K4E::Vector3& startPos, const std::string& modelPath = "cube.gltf");

	// 互換用: 旧シーンが Update() を呼んでも落ちないようにする（dtは仮値）
	void Update() { Update(1.0f / 60.0f); }
	void Update(float dt) override;

	// 依存の注入
	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetBulletManager(BulletManager* bm) { bulletManager_ = bm; }

	// ---- FSMから参照される query ----
	bool  IsInAttackRange(float distToPlayer) const { return distToPlayer <= attackRange_; }
	float GetFireInterval() const { return fireInterval_; }

	// ---- 行動(命令実行) ----
	void MoveTowards(const K4E::Vector3& goal);
	void StopMove();
	void FaceTo(const K4E::Vector3& lookAt);
	void FireAt(const K4E::Vector3& targetPos);

	// スタン要求（被弾などから呼ぶ）
	void RequestStun(float sec);
	float ConsumeStunDurationOr(float fallbackSec);

	EnemyStateId GetStateId() const { return fsm_.GetStateId(); }

protected:
	// EnemyBaseからの弾ヒット
	void OnBulletHit(K4E::Collider* bulletCollider) override;

private:
	void ApplyAICommand(const EnemyAICommand& cmd);
	void BuildContext(EnemyAIContext<Enemy>& ctx);

private:
	EnemyStateMachine<Enemy> fsm_{};

	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;

	// perception / memory
	float viewRange_ = 25.0f;
	K4E::Vector3 lastSeenPos_{ 0.0f,0.0f,0.0f };
	float timeSinceSeen_ = 9999.0f;

	// movement / combat config
	float moveSpeed_ = 3.0f;      // units/sec
	float attackRange_ = 15.0f;
	float fireInterval_ = 0.35f;
	float bulletSpeed_ = 18.0f;
	float bulletLifeSec_ = 3.0f;
	int   bulletDamage_ = 1;
	float muzzleHeight_ = 1.2f;

	// stun request
	float stunRequestedSec_ = 0.0f;
};
