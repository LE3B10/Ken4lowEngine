#pragma once
#include "Vector3.h"


/// ---------- 前方宣言 ---------- ///
class Input;
class Camera;
class AnimationModel;


/// -------------------------------------------------------------
///					　プレイヤーコントローラー
/// -------------------------------------------------------------
class PlayerController
{
private: /// ---------- 構造体 ---------- ///

	// 行動状態を管理する構造体
	struct MovementConfig
	{
		float baseSpeed = 2.0f;			// 基本移動速度
		float dashMultiplier = 3.0f;	// ダッシュ時の速度倍率
		float adsSpeedFactor = 0.5f;	// エイムダウンサイト時の移動速度倍率
		float crouchSpeedFactor = 0.5f;	// しゃがみ時の移動速度倍率
	};

	// ジャンプ状態を管理する構造体
	struct JumpConfig
	{
		float gravity = -0.98f;	 // 重力加速度
		float jumpPower = 0.28f; // ジャンプ力
		float jumpCost = 15.0f;  // ジャンプ時のスタミナ消費量
	};

	// スタミナ状態を管理する構造体
	struct StaminaState
	{
		float* current = nullptr;  // スタミナへのポインタ
		float max = 100.0f;		   // 最大スタミナ
		float regenRate = 15.0f;   // スタミナ回復速度
		float dashCost = 20.0f;	   // ダッシュ時のスタミナ消費量
		bool isBlocked = false;	   // スタミナ回復ブロックフラグ
		float recoverDelay = 1.0f; // スタミナ回復遅延時間
		float recoverTimer = 0.0f; // スタミナ回復タイマー
	};

	// 入力状態を管理する構造体
	struct InputFlags
	{
		bool jump = false;		   // ジャンプ入力フラグ
		bool crouch = false;	   // しゃがみ入力フラグ
		bool dash = false;		   // ダッシュ中フラグ
		bool aim = false;		   // エイムダウンサイト入力フラグ
		bool triggerShoot = false; // 射撃中フラグ（単発）
		bool holdShoot = false;	   // 射撃中フラグ（連発）
	};

	// ステータス状態を管理する構造体
	struct StatusFlags
	{
		bool isGrounded = true;		// 地面に接地しているかどうか
		bool isDashing = false;		// ダッシュ中フラグ
		bool isDebugCamera = false; // デバッグカメラフラグ
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(AnimationModel* model);

	// 更新処理
	void UpdateMovement(Camera* camera, float deltaTime, bool weaponReloading);

	// ImGuiでの移動デバッグ表示
	void DrawMovementImGui();

public: /// ---------- アクセサー関数 ---------- ///

	// 移動入力を取得
	Vector3 GetMoveInput() const { return move_; }

	// ジャンプ入力を取得
	bool IsJumpTriggered() const { return inputFlags_.jump; }

	// しゃがみ入力を取得
	bool IsCrouching() const { return inputFlags_.crouch; }
	void SetCrouchInput(bool crouch) { inputFlags_.crouch = crouch; }

	// エイムダウンサイト入力を取得
	bool IsAimingInput() const { return inputFlags_.aim; }
	void SetAimingInput(bool aiming) { inputFlags_.aim = aiming; }

	// 単発射撃入力を取得
	bool IsTriggerShooting() const { return inputFlags_.triggerShoot; }

	// 連発射撃入力を取得
	bool IsPushShooting() const { return inputFlags_.holdShoot; }

	// 地面に接地しているかどうかを取得
	bool IsGrounded() const { return statusFlags_.isGrounded; }

	// ダッシュ中かどうかを取得
	bool IsDashing() const { return statusFlags_.isDashing; }

	// スタミナの現在値を取得
	float GetCurrentStamina() const { return staminaState_.current ? *staminaState_.current : 0.0f; }
	void SetStaminaPointer(float* stamina) { staminaState_.current = stamina; }

	// スタミナの最大値を取得
	float GetMaxStamina() const { return staminaState_.max; }

	// スタミナの回復速度を取得
	float GetStaminaRegenRate() const { return staminaState_.regenRate; }

	// スタミナのダッシュコストを取得
	float GetStaminaDashCost() const { return staminaState_.dashCost; }

	void SetShooting(bool shooting) { inputFlags_.triggerShoot = shooting; }

	// デバッグカメラフラグ
	bool IsDebugCamera() const { return statusFlags_.isDebugCamera; }
	void SetDebugCamera(bool isDebugCamera) { statusFlags_.isDebugCamera = isDebugCamera; }

private: /// ---------- メンバ変数 ---------- ///

	// スタミナの更新処理
	void UpdateStamina(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	AnimationModel* animationModel_ = nullptr;
	Input* input_ = nullptr;

	Vector3 velocity_{};
	Vector3 move_{};

	// 行動状態
	MovementConfig movementConfig_{};

	// ジャンプ状態
	JumpConfig jumpConfig_{};

	// スタミナ状態
	StaminaState staminaState_{};

	// 入力状態
	InputFlags inputFlags_{};

	// ステータス状態
	StatusFlags statusFlags_{};
};

