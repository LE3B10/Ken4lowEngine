#pragma once
#include "Vector3.h"
#include "EnemyBase.h"
#include "IEnemyState.h"

#include <memory>

/// ----------前方宣言 ---------- ///
class BulletManager;
class CollisionManager;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///							Enemy
/// -------------------------------------------------------------
class Enemy final : public EnemyBase
{
private: /// ---------- 構造体 ---------- ///

	// 視覚・感知の設定
	struct EnemyPerceptionConfig
	{
		float viewRange = 25.0f;
		float viewFovDeg = 120.0f;         // 視野角（左右合計）
		float viewFovVerticalDeg = 85.0f;  // 縦（上下合計）
		float eyeHeight = 1.2f;            // 目の高さ
		float targetEyeHeight = 1.2f;      // ターゲット側の高さ
		bool useLOS = true;                // 遮蔽チェックをするか
		bool useVerticalFov = false;       // まずは縦FOVを無効化して取りこぼしを減らす
		float nearDetectRadius = 6.0f;     // この距離以内は横FOVを無視して気付きやすくする
		float nearLoseRadius = 7.5f;       // 近距離で見失いにくくするヒステリシス
	};

	// 移動 / 戦闘の設定
	struct EnemyCombatConfig
	{
		float moveSpeed = 3.0f;		// 移動速度
		float attackRange = 15.0f;  // 攻撃可能距離（射程）
		float fireInterval = 0.35f; // 攻撃間隔（秒）
		float bulletSpeed = 18.0f;  // 弾の速度
		float bulletLifeSec = 3.0f; // 弾の寿命（秒）
		int   bulletDamage = 50;    // 弾のダメージ
		float muzzleHeight = 1.2f;  // マズルの高さ
	};

	// 敵の記憶
	struct EnemyMemory
	{
		K4E::Vector3 lastSeenPos{ 0.0f, 0.0f, 0.0f }; // 最後に見たターゲットの位置
		float timeSinceSeen = 9999.0f;				  // 最後に見てからの経過時間（秒）
	};

	// 敵の向き（Yawのみ）
	struct EnemyFacing
	{
		float yawRad = 0.0f; // Yaw角（ラジアン）
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	void Update(float deltaTime) override;

public: /// ---------- 外部からのアクセス ---------- ///

	// 依存の注入
	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetBulletManager(BulletManager* bm) { bulletManager_ = bm; }
	void SetCollisionManager(CollisionManager* cm) { collisionManager_ = cm; }

public: /// ---------- 状態管理 ---------- ///

	void ChangeState(std::unique_ptr<IEnemyState> nextState);
	void ChangeStateToIdle();
	void ChangeStateToCombatMove();
	void ChangeStateToShoot();
	void ChangeStateToSearch();
	void ChangeStateToDead();

public: /// ---------- アクセサ ---------- ///

	bool HasTarget() const { return target_ != nullptr; }
	K4E::Vector3 GetTargetPosition() const;
	float GetDistanceToTarget() const;

	void StopMove();
	void MoveTowards(const K4E::Vector3& targetPos);
	void FaceTo(const K4E::Vector3& targetPos);
	void FireAt(const K4E::Vector3& targetPos);

	void RememberLastSeenTarget(const K4E::Vector3& targetPos)
	{
		memory_.lastSeenPos = targetPos;
		memory_.timeSinceSeen = 0.0f;
	}

	const K4E::Vector3& GetLastSeenTargetPosition() const { return memory_.lastSeenPos; }

	bool CanSeeTargetPublic(const K4E::Vector3& targetPos, float distToTarget) { return CanSeeTarget(targetPos, distToTarget); }

	bool CanShootTargetPublic(const K4E::Vector3& targetPos) const { return CanShootTarget(targetPos); }

	float GetAttackRange() const { return combat_.attackRange; }
	float GetMoveSpeed() const { return combat_.moveSpeed; }
	float GetFireInterval() const { return combat_.fireInterval; }

protected: /// ---------- EnemyBaseからの通知 ---------- ///

	// EnemyBaseからの弾ヒット
	void OnBulletHit(K4E::Collider* bulletCollider) override;

private: /// ---------- 視界判定 ---------- ///

	// 視覚判定
	bool CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget);

	// 射線判定
	bool CanShootTarget(const K4E::Vector3& targetPos) const;

private: /// ---------- メンバ変数 ---------- ///

	// 外部依存
	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;
	CollisionManager* collisionManager_ = nullptr;

	// 視覚 / 記憶 / 移動 / 戦闘の設定
	EnemyPerceptionConfig perception_{};

	// 移動 / 戦闘の設定
	EnemyCombatConfig combat_{};

	// 敵の記憶
	EnemyMemory memory_{};

	// 敵の向き（Yawのみ）
	EnemyFacing facing_{};

	std::unique_ptr<IEnemyState> state_ = nullptr;
	float fireCooldown_ = 0.0f; // 攻撃間隔のクールダウン
};
