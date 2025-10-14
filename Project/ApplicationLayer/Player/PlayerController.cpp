#define NOMINMAX
#include "PlayerController.h"
#include <Input.h>
#include <AnimationModel.h>
#include <Camera.h>
#include <Object3D.h>

#include <algorithm> // std::clamp, std::max
#include <cmath>     // std::sinf, std::cosf, std::sqrt
#include <imgui.h>

/// -------------------------------------------------------------
///                         初期化処理
/// -------------------------------------------------------------
void PlayerController::Initialize(AnimationModel* model)
{
	input_ = Input::GetInstance();
	animationModel_ = model;
	velocity_ = { 0.0f, 0.0f, 0.0f };
	move_ = { 0.0f, 0.0f, 0.0f };
	statusFlags_.isGrounded = true;   // 初期は接地
	fovYCurrent_ = fovYBase_;         // FOV初期化
	isSliding_ = false;
	coyoteTimer_ = 0.0f;
	jumpBuffer_ = 0.0f;
	fovInit_ = false;
}

/// -------------------------------------------------------------
///                          更新処理
/// -------------------------------------------------------------
void PlayerController::UpdateMovement(Camera* camera, float deltaTime, bool weaponReloading)
{
	if (IsDebugCamera()) return;

	// 移動
	move_ = { 0,0,0 };
	if (input_->PushKey(DIK_W)) move_.z += 1.0f;
	if (input_->PushKey(DIK_S)) move_.z -= 1.0f;
	if (input_->PushKey(DIK_D)) move_.x += 1.0f;
	if (input_->PushKey(DIK_A)) move_.x -= 1.0f;

	// ジャンプ（先行入力を維持）
	inputFlags_.jump = input_->TriggerKey(DIK_SPACE) || inputFlags_.jump;
	inputFlags_.crouch = input_->PushKey(DIK_LCONTROL);

	// ADS（右クリック＝エイム専用）
	inputFlags_.aim = input_->PushMouse(1);

	// 射撃（左クリックのみを使用）★ここが修正点
	inputFlags_.holdShoot = input_->PushMouse(0);     // 連射
	inputFlags_.triggerShoot = input_->TriggerMouse(0);  // 単発

	// ダッシュキー
	bool wantSprintKey = input_->PushKey(DIK_LSHIFT);

	if (Vector3::Length(move_) > 0.0f) {
		move_ = Vector3::Normalize(move_);
	}

	// === カメラのヨーで移動ベクトルをワールドへ ===
	float yaw = 0.0f;
	if (camera) {
		yaw = camera->GetRotate().y;
		float s = std::sinf(yaw), c = std::cosf(yaw);
		move_ = { move_.x * c - move_.z * s, 0.0f, move_.x * s + move_.z * c };
	}
	animationModel_->SetRotate({ 0.0f, yaw, 0.0f });

	// === スプリント（ADS中は無効・全方向対応：移動入力があるとき） ===
	const bool isADS = (!weaponReloading && inputFlags_.aim);
	const bool isAnyMove = (Vector3::Length(move_) > 0.01f);
	const bool isSprint = (wantSprintKey && !isADS && isAnyMove);
	statusFlags_.isDashing = isSprint;

	// === 速度ターゲット（Apexっぽく：基礎速度×状態係数） ===
	const float walkSpeed = 6.0f;
	const float sprintSpeed = 8.8f;
	const float adsScale = 0.65f;
	const float crouchScale = 0.5f;

	float targetSpeed = (isSprint ? sprintSpeed : walkSpeed);
	if (isADS)               targetSpeed *= adsScale;
	if (inputFlags_.crouch)  targetSpeed *= crouchScale;

	Vector3 targetVel = move_ * targetSpeed;

	// === 加速・減速（慣性） ===
	const float groundAccel = 26.0f;
	const float groundFriction = 10.0f;
	const float airAccel = 4.0f;
	const float airFriction = 0.2f;

	auto clampLen = [](const Vector3& v, float maxLen) {
		float len = Vector3::Length(v);
		if (len <= maxLen || len <= 1e-6f) return v;
		return v * (maxLen / len);
		};
	auto applyFrictionXZ = [](Vector3& v, float amount) {
		Vector3 xz{ v.x,0,v.z };
		float l = Vector3::Length(xz); if (l <= 1e-6f) return;
		float nl = std::max(0.0f, l - amount);
		if (nl == l) return;
		Vector3 dir = xz / l;
		Vector3 nxz = dir * nl;
		v.x = nxz.x; v.z = nxz.z;
		};

	// === スライド（速度しきい値＋しゃがみで開始、低摩擦で伸びる） ===
	const float slideMinSpeed = 6.5f;
	const float slideFriction = 0.6f;

	// === 位置・重力 ===
	Vector3 pos = animationModel_->GetTranslate();

	if (statusFlags_.isGrounded) {
		// スライド開始
		float speedXZ = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
		if (!isSliding_ && inputFlags_.crouch && speedXZ > slideMinSpeed) {
			isSliding_ = true;
		}

		// 地上加速
		Vector3 dv = targetVel - velocity_;
		velocity_ += clampLen(dv, groundAccel * deltaTime);

		// 摩擦（スライド中は弱め）
		if (isSliding_) applyFrictionXZ(velocity_, slideFriction * deltaTime);
		else            applyFrictionXZ(velocity_, groundFriction * deltaTime);

		// スライド解除条件
		speedXZ = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
		if (!inputFlags_.crouch || speedXZ < 2.5f) isSliding_ = false;
	}
	else {
		// 空中加速（エアストレイフ風）
		velocity_ += move_ * (airAccel * deltaTime);
		applyFrictionXZ(velocity_, airFriction * deltaTime);
	}

	// === コヨーテタイム & ジャンプバッファ（“許し”） ===
	const float kCoyoteTime = 0.12f;
	const float kJumpBuffer = 0.12f;
	const float gravity = 18.0f;
	const float jumpSpeed = 5.5f;

	coyoteTimer_ = statusFlags_.isGrounded ? kCoyoteTime : std::max(0.0f, coyoteTimer_ - deltaTime);
	jumpBuffer_ = inputFlags_.jump ? kJumpBuffer : std::max(0.0f, jumpBuffer_ - deltaTime);

	if (jumpBuffer_ > 0.0f && coyoteTimer_ > 0.0f) {
		jumpBuffer_ = 0.0f;
		coyoteTimer_ = 0.0f;
		velocity_.y = jumpSpeed;
		statusFlags_.isGrounded = false;
		inputFlags_.jump = false; // 消費
	}

	// 重力
	velocity_.y -= gravity * deltaTime;

	// 位置更新
	pos += velocity_ * deltaTime;

	// 簡易接地
	if (pos.y <= 0.0f) {
		pos.y = 0.0f;
		velocity_.y = 0.0f;
		statusFlags_.isGrounded = true;
	}
	else {
		statusFlags_.isGrounded = false;
	}

	// 反映
	animationModel_->SetTranslate(pos);

	// === FOV補間（ADSで狭く・ダッシュ中は少し広く） ===
	if (camera) {
		if (!fovInit_) {
			// 初回のみ既定値を使用（カメラ側にゲッターが無くてもOK）
			fovYCurrent_ = fovYBase_;
			fovInit_ = true;
		}

		const float targetFov = isADS
			? fovYADS_
			: ((isSprint && isAnyMove) ? fovYBase_ * fovYSprintMul_ : fovYBase_);

		float t = std::clamp(fovLerpSpeed_ * deltaTime, 0.0f, 1.0f);
		fovYCurrent_ += (targetFov - fovYCurrent_) * t;
		camera->SetFovY(fovYCurrent_);
	}
}

/// -------------------------------------------------------------
///                     スタミナの更新処理（残置）
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
///                    ImGuiでの移動デバッグ表示
/// -------------------------------------------------------------
void PlayerController::DrawMovementImGui()
{
	if (!animationModel_) return;

	ImGui::Text("== Movement Debug ==");
	Vector3 pos = animationModel_->GetTranslate();
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
	ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity_.x, velocity_.y, velocity_.z);
	ImGui::Text("Grounded: %s", statusFlags_.isGrounded ? "Yes" : "No");
	ImGui::Text("Dashing : %s", statusFlags_.isDashing ? "Yes" : "No");
	if (staminaState_.current) ImGui::Text("Stamina : %.2f / %.2f", *staminaState_.current, staminaState_.max);
}
