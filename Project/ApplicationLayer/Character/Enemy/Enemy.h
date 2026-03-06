#pragma once
#include "Vector3.h"
#include <string>
#include <functional>

#include "EnemyBase.h"
#include "EnemyAICommand.h"
#include "EnemyStateMachine.h"
#include "EnemyGunAI.h"
#include "EnemyArchetype.h"

// 前方宣言
class BulletManager;
class CollisionManager;

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// Enemy
///  - EnemyBase の物理（位置/速度）に、意思決定(FSM)と射撃を追加
///  - FSMは「命令（EnemyAICommand）」だけを出し、Enemyが実行する
/// -------------------------------------------------------------
class Enemy final : public EnemyBase
{
public: /// ---------- メンバ関数 ---------- ///

	Enemy() = default;
	~Enemy() override = default;

	// 互換用: 旧シーンが Initialize() を呼んでも動くようにする
	void Initialize() { Initialize({ 0.0f, 1.5f, 30.0f }); }

	void Initialize(const K4E::Vector3& startPos);

	// 互換用: 旧シーンが Update() を呼んでも落ちないようにする（dtは仮値）
	void Update() { Update(1.0f / 60.0f); }
	void Update(float dt) override;

	// 描画処理
	void Draw() override;

	// ImGuiの描画処理
	void DrawImGui() override;

	// シャドウマップ用行列の更新
	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection) override;

	// シャドウマップ描画処理
	void DrawShadow() override;

public: /// ---------- 外部からのアクセス ---------- ///

	// 依存の注入
	void SetTarget(K4E::Collider* target) { target_ = target; }
	void SetBulletManager(BulletManager* bm) { bulletManager_ = bm; }
	void SetCollisionManager(CollisionManager* cm) { collisionManager_ = cm; }

	// UI通知（プレイヤー側/HUD側を外から注入）
	void SetOnPlayerHitUICallback(std::function<void(bool isHeadshot)> cb) { onPlayerHitUICallback_ = std::move(cb); }
	void SetOnPlayerKillUICallback(std::function<void(bool isHeadshot)> cb) { onPlayerKillUICallback_ = std::move(cb); }

	// ---- FSMから参照される query ----
	bool  IsInAttackRange(float distToPlayer) const { return distToPlayer <= attackRange_; }
	float GetFireInterval() const { return fireInterval_; }

	EnemyStateId GetStateId() const { return fsm_.GetStateId(); }

	float GetMoveSpeed() const { return moveSpeed_; }
	float GetAttackRange() const { return attackRange_; }

	const K4E::Vector3& GetHomePos() const { return homePos_; }

	// ---- Archetype / tuning ----
	void SetArchetype(EnemyArchetype t);
	EnemyArchetype GetArchetype() const { return archetype_; }
	const EnemyTuning& GetTuning() const { return tuning_; }

	void SetDebugCamera(bool enabled) { debugCamera_ = enabled; }

public: /// ---------- FSMから呼ばれる行動命令 ---------- ///

	// ---- 行動(命令実行) ----
	void MoveTowards(const K4E::Vector3& goal);
	void StopMove();
	void FaceTo(const K4E::Vector3& lookAt);
	void FireAt(const K4E::Vector3& targetPos);

	// スタン要求（被弾などから呼ぶ）
	void RequestStun(float sec);
	float ConsumeStunDurationOr(float fallbackSec);

protected: /// ---------- EnemyBaseからの通知 ---------- ///

	// EnemyBaseからの弾ヒット
	void OnBulletHit(K4E::Collider* bulletCollider) override;

private: /// ---------- AICommand ---------- ///

	void ApplyAICommand(const EnemyAICommand& cmd);
	void BuildContext(EnemyAIContext<Enemy>& ctx);

private: /// ---------- 視界判定 ---------- ///

	// 視覚判定
	bool CanSeeTarget(const K4E::Vector3& targetPos, float distToTarget);

	// 射線判定（発砲できるか：マズル→ターゲットが壁に当たらない）
	bool CanShootTarget(const K4E::Vector3& targetPos) const;

	// ワイヤー描画
	void DrawVisionWire() const;

private: /// ---------- メンバ変数 ---------- ///

	EnemyStateMachine<Enemy> fsm_{};

	K4E::Collider* target_ = nullptr;
	BulletManager* bulletManager_ = nullptr;

	CollisionManager* collisionManager_ = nullptr;

	K4E::Vector3 homePos_{ 0.0f, 0.0f, 0.0f };

	// archetype / tuning
	EnemyArchetype archetype_ = EnemyArchetype::RifleGrunt;
	EnemyTuning tuning_{};

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

	// ---- Vision params ----
	float viewFovDeg_ = 120.0f;          // 視野角（左右合計）
	float viewFovVerticalDeg_ = 85.0f;   // 縦（上下合計）
	float eyeHeight_ = 1.2f;             // 目の高さ
	float targetEyeHeight_ = 1.2f;       // ターゲット側の高さ（雑に同じでもOK）
	bool  useLOS_ = true;                // 遮蔽チェックをするか
	float nearDetectRadius_ = 6.0f;      // この距離以内は横FOVを無視して気付きやすくする
	float nearLoseRadius_ = 7.5f;        // 近距離で見失いにくくするヒステリシス
	bool  useVerticalFov_ = false;       // まずは縦FOVを無効化して取りこぼしを減らす

	// ---- Facing----
	float yawRad_ = 0.0f;           // FaceToで更新
	float pitchRad_ = 0.0f; 	   // 将来の拡張用

	// ---- Debug draw ----
	bool  debugDrawVision_ = true;
	int   debugVisionSegments_ = 24;

	// デバッグ用に直近の結果を保持（任意）
	bool  lastCanSee_ = false;
	K4E::Vector3 lastPlayerPos_{};
	bool  lastDistOk_ = false;
	bool  lastHorizOk_ = false;
	bool  lastVertOk_ = false;
	bool  lastLosOk_ = false;
	bool  lastNearBypass_ = false;

	bool debugCamera_ = false;

	std::function<void(bool isHeadshot)> onPlayerHitUICallback_{};
	std::function<void(bool isHeadshot)> onPlayerKillUICallback_{};// UI通知用コールバック
};
