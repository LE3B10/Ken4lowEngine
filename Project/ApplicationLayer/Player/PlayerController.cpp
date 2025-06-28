#include "PlayerController.h"
#include <Input.h>
#include <AnimationModel.h>
#include <Camera.h>

#include <imgui.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void PlayerController::Initialize(AnimationModel* model)
{
	input_ = Input::GetInstance();
	animationModel_ = model;
	velocity_ = { 0.0f, 0.0f, 0.0f };
	move_ = { 0.0f, 0.0f, 0.0f };
	statusFlags_.isGrounded = true; // 初期状態では地面に接地している
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void PlayerController::UpdateMovement(Camera* camera, float deltaTime, bool weaponReloading)
{
	Vector3 moveInput = { 0.0f, 0.0f, 0.0f }; // ← ★リセットされた移動入力
	move_ = moveInput;                        // ← ★現在の入力で move_ を更新

	if (input_->PushKey(DIK_W)) move_.z += 1.0f;
	if (input_->PushKey(DIK_S)) move_.z -= 1.0f;
	if (input_->PushKey(DIK_D)) move_.x += 1.0f;
	if (input_->PushKey(DIK_A)) move_.x -= 1.0f;
	if (input_->PushKey(DIK_LSHIFT)) inputFlags_.dash = true; // ダッシュ入力
	if (input_->TriggerKey(DIK_SPACE)) inputFlags_.jump = true; // ジャンプ入力

	if (input_->PushKey(DIK_LCONTROL)) inputFlags_.crouch = true; // しゃがみ入力
	else inputFlags_.crouch = false; // しゃがみ入力をリセット

	// 1. 右クリックを押している場合、エイムダウンサイト入力を有効にする
	if (input_->PushMouse(1)) inputFlags_.aim = true; // エイムダウンサイト入力
	else inputFlags_.aim = false; // エイム入力をリセット

	inputFlags_.holdShoot = input_->PushMouse(0); // 連発射撃入力（マウス左クリック）
	inputFlags_.triggerShoot = input_->TriggerMouse(0); // 単発射撃入力（マウス左クリック）

	// 正規化
	if (Vector3::Length(move_) > 0.0f) move_ = Vector3::Normalize(move_);

	// 入力状態の更新
	if (inputFlags_.aim || weaponReloading)
	{
		statusFlags_.isDashing = false;
		move_ *= 0.5f;
	}
	else if (inputFlags_.dash && (move_.x != 0.0f || move_.z != 0.0f) && *staminaState_.current >= staminaState_.dashCost * deltaTime)
	{
		statusFlags_.isDashing = true;
		*staminaState_.current -= staminaState_.dashCost * deltaTime;
		staminaState_.recoverTimer = 0.0f;
		staminaState_.isBlocked = true;
	}
	else
	{
		statusFlags_.isDashing = false;
		inputFlags_.dash = false; // ダッシュ入力をリセット
	}

	// カメラの向きで移動変換
	float yaw = 0.0f;
	if (camera)
	{
		yaw = camera->GetRotate().y;
		float sinY = sinf(yaw);
		float cosY = cosf(yaw);
		move_ = {
			move_.x * cosY - move_.z * sinY,
			0.0f,
			move_.x * sinY + move_.z * cosY
		};
	}

	// アニメーションモデルの向きを更新
	animationModel_->SetRotate({ 0.0f, yaw, 0.0f });

	bool isAimingNow = (!weaponReloading && inputFlags_.aim); // エイムダウンサイト中かどうか
	float speed = movementConfig_.baseSpeed;

	if (statusFlags_.isDashing) speed *= movementConfig_.dashMultiplier; // ダッシュ中は速度を上げる

	// エイムダウンサイト中の速度調整
	if (isAimingNow) speed *= movementConfig_.adsSpeedFactor;

	if (camera) camera->SetFovY(isAimingNow ? 0.5f : 1.0f);  // ADS時のFOV調整

	if (inputFlags_.crouch) speed *= movementConfig_.crouchSpeedFactor;  // しゃがみ中は速度を下げる

	Vector3 pos = animationModel_->GetTranslate();
	pos += move_ * speed * deltaTime;

	// ジャンプ処理
	if (*staminaState_.current > 0.0f && statusFlags_.isGrounded && inputFlags_.jump)
	{
		velocity_.y = jumpConfig_.jumpPower;
		statusFlags_.isGrounded = false;
		*staminaState_.current -= jumpConfig_.jumpCost;
		staminaState_.isBlocked = true;
		staminaState_.recoverTimer = 0.0f;
	}

	velocity_.y += jumpConfig_.gravity * deltaTime;
	pos.y += velocity_.y;

	if (pos.y <= 0.0f)
	{
		pos.y = 0.0f;
		velocity_.y = 0.0f;
		statusFlags_.isGrounded = true;
		inputFlags_.jump = false; // ジャンプ入力をリセット
	}

	// アニメーションモデルの位置を更新
	animationModel_->SetTranslate(pos);

	// スタミナの更新処理
	UpdateStamina(deltaTime);
}


/// -------------------------------------------------------------
///				　		スタミナの更新処理
/// -------------------------------------------------------------
void PlayerController::UpdateStamina(float deltaTime)
{
	if (staminaState_.isBlocked)
	{
		staminaState_.recoverTimer += deltaTime;
		if (staminaState_.recoverTimer >= staminaState_.recoverDelay)
		{
			staminaState_.isBlocked = false;
			staminaState_.recoverTimer = 0.0f;
		}
	}

	if (!staminaState_.isBlocked && staminaState_.current)
	{
		*staminaState_.current += staminaState_.regenRate * deltaTime;
		if (*staminaState_.current > staminaState_.max) {
			*staminaState_.current = staminaState_.max;
		}
	}
}


/// -------------------------------------------------------------
///				　	 ImGuiでの移動デバッグ表示
/// -------------------------------------------------------------
void PlayerController::DrawMovementImGui()
{
	if (!animationModel_) return;

	ImGui::Text("== Movement Debug ==");
	Vector3 pos = animationModel_->GetTranslate();
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity_.x, velocity_.y, velocity_.z);
	ImGui::Text("Grounded: %s", statusFlags_.isGrounded ? "Yes" : "No");
	ImGui::Text("Dashing: %s", statusFlags_.isDashing ? "Yes" : "No");
	if (staminaState_.current) ImGui::Text("Stamina: %.2f / %.2f", *staminaState_.current, staminaState_.max);
}