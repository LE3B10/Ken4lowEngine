#pragma once
#include "Vector3.h"

/// ---------- 前方宣言 ---------- ///
class AnimationModel;
class PlayerController;
class Camera;


/// -------------------------------------------------------------
///					　プレイヤー移動コントローラー
/// -------------------------------------------------------------
class PlayerMovementController
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(AnimationModel* model);

	// 更新処理
	void Update(const PlayerController* controller, Camera* camera, float deltaTime, bool weaponReloading);

	// ImGui描画
	void DrawImGui();

	// 着地状態を設定
	bool IsGrounded() const { return isGrounded_; }

	// ダッシュ入力（キー押し状態）をセット
	void SetDashInput(bool dashInput) { isDashing_ = dashInput; }
	// ダッシュ状態を取得
	bool IsDashing() const { return isDashing_; }

	// スタミナを渡す
	void SetStamina(float* stamina) { stamina_ = stamina; }
	// スタミナを取得
	float GetStamina() const { return *stamina_; }
	// 最大スタミナを取得
	float GetMaxStamina() const { return maxStamina_; }

	// ADS入力をセット
	void SetAimingInput(bool aimingInput) { isAimingInput_ = aimingInput; }
	bool IsAimingInput() const { return isAimingInput_; }

	// しゃがみ入力をセット
	void SetCrouchInput(bool crouchInput) { isCrouchInput_ = crouchInput; }
	// しゃがみ状態を取得
	bool IsCrouchInput() const { return isCrouchInput_; }

private: /// ---------- メンバ変数 ---------- ///

	// スタミナの更新処理
	void UpdateStamina(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	AnimationModel* animationModel_ = nullptr;
	Vector3 velocity_;
	bool isGrounded_; // 地面にいるかどうか
	bool isDashing_ = false; // ダッシュ中かどうか

	float gravity_ = -0.98f;
	float jumpPower_ = 0.28f;

	float baseSpeed_ = 2.0f;
	float dashMultiplier_ = 3.0f;
	float adsSpeedFactor_ = 0.5f;
	float crouchSpeedFactor_ = 0.5f;

	float* stamina_ = nullptr;        // 現在のスタミナ
	float maxStamina_ = 100.0f;     // 最大スタミナ
	float staminaRegenRate_ = 15.0f; // 1秒あたりの回復量
	float staminaDashCost_ = 20.0f;  // ダッシュ1回分の消費量
	float staminaJumpCost_ = 15.0f;  // ジャンプ1回分の消費量
	bool isStaminaRecoverBlocked_ = false; // 一時的に回復を止める（行動中など）
	float staminaRecoverDelay_ = 1.0f; // 行動後、回復開始までの猶予秒数
	float staminaRecoverTimer_ = 0.0f; // スタミナ回復タイマー

	bool isAimingInput_ = false; // エイミング入力（ADS）状態
	bool isCrouchInput_ = false; // しゃがみ入力状態
};
