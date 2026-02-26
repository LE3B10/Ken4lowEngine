#pragma once
#include "WorldTransformEx.h"
#include <functional>

#include "PlayerStateMachines.h" // LocoId / InputSnapshot

namespace K4E = ::Ken4lowEngine;

/**************************************************
 * Player の「移動・重力・地面スナップ・ジャンプ/ダッシュ補助」を担当。
 * - FSM は “何をしたいか” を決めるだけ
 * - Motor は “どう動くか(物理/積分/スナップ)” を担当
 **************************************************/
class PlayerMotorComponent
{
public: /// ---------- インターフェース ---------- ///

	// セットアップ : PlayerAPI から呼ぶ想定
	using GroundQueryFn = std::function<bool(const K4E::Vector3& origin, float maxDistance, float& outGroundY, K4E::Vector3& outNormal)>;

public: /// ---------- パブリックメンバ関数 ---------- ///

	// ===== Setup =====
	void BindTransform(K4E::WorldTransformEx* tr) { tr_ = tr; }
	void SetGroundQuery(GroundQueryFn fn) { groundQuery_ = std::move(fn); }

	// ===== Frame hooks =====
	// JumpBuffer / Coyote / dash cooldown など “入力前処理” を行う（brain.Update の前に呼ぶ）
	void PreprocessInput(InputSnapshot& in, float dt);

	// 移動・重力の積分（brain.Update の後に呼ぶ）
	void Simulate(float dt, float cameraYawRad, LocoId locoId, bool isAds, bool isReloading);

	// ===== Queries (for FSM) =====
	bool IsGrounded() const;
	bool IsSprinting() const { return sprint_; }
	float VerticalVelocity() const { return verticalVel_; }
	bool CanStartDash() const { return (dashCooldownTimer_ <= 0.0f) && (dashTimer_ <= 0.0f); }
	bool IsDashFinished() const { return dashTimer_ <= 0.0f; }

	// ===== Commands (from FSM via PlayerAPI) =====
	void SetMoveInput(float x, float z) { moveX_ = x; moveZ_ = z; }
	void SetSprint(bool on) { sprint_ = on; }
	void Jump();
	void StartDash(float cameraYawRad, bool isAds);

	// ===== Parameters (ImGui で触るなら getter/setter 生やす) =====
	float& WalkSpeed() { return walkSpeed_; }
	float& RunSpeed() { return runSpeed_; }
	float& DashSpeed() { return dashSpeed_; }
	float& JumpSpeed() { return jumpSpeed_; }
	float& Gravity() { return gravity_; }

	float GetSpeedXZ_Debug() const { return dbgSpeedXZ_; }
	float GetSpeedY_Debug()  const { return dbgSpeedY_; }

	void SetAdsMoveMultiplier(float mul) { adsMoveMul_ = (mul < 0.0f) ? 0.0f : mul; }

private: /// ---------- プライベートメンバ関数 ---------- ///

	// 地面クエリから groundY_ と groundNormal_ を更新する
	void UpdateGroundFromQuery();

private: /// ---------- プライベートメンバ変数 ---------- ///

	K4E::WorldTransformEx* tr_ = nullptr;
	GroundQueryFn groundQuery_{};

	// ---- Ground ----
	float groundY_ = 0.0f;
	K4E::Vector3 groundNormal_{ 0,1,0 };
	float groundSnapEpsilon_ = 0.02f;
	float groundProbeDistance_ = 3.0f;
	float stepSnapHeight_ = 0.45f;

	// ---- Input from FSM ----
	float moveX_ = 0.0f;
	float moveZ_ = 0.0f;
	bool  sprint_ = false;

	// ---- Velocity ----
	float velX_ = 0.0f;
	float velZ_ = 0.0f;
	float verticalVel_ = 0.0f;

	// ---- Dash ----
	float dashTimer_ = 0.0f;
	float dashDuration_ = 0.25f; // ダッシュの持続時間
	float dashDirX_ = 0.0f;
	float dashDirZ_ = 1.0f;
	float dashCooldownTimer_ = 0.0f;
	float dashCooldown_ = 3.0f;

	// ---- Jump feel ----
	float coyoteTimer_ = 0.0f;
	float coyoteTime_ = 0.10f;
	float jumpBufferTimer_ = 0.0f;
	float jumpBufferTime_ = 0.10f;

	// ---- Motor tuning ----
	float accelGround_ = 28.0f;
	float decelGround_ = 32.0f;
	float accelAir_ = 12.0f;

	float adsMoveMul_ = 0.25f; // ADS中の移動減速
	float reloadMoveMul_ = 0.1f; // リロード中の移動減速
	float airControl_ = 0.7f;

	// ---- Speeds ----
	float walkSpeed_ = 3.0f;
	float runSpeed_ = 6.0f;
	float dashSpeed_ = 15.0f;
	float jumpSpeed_ = 8.0f;
	float gravity_ = 19.6f;

	// ---- Debug speed ----
	K4E::Vector3 prevPos_{ 0,0,0 };
	bool prevPosValid_ = false;
	float dbgSpeedXZ_ = 0.0f;
	float dbgSpeedY_ = 0.0f;
};

