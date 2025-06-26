#include "PlayerMovementController.h"
#include "AnimationModel.h"
#include "PlayerController.h"
#include "Camera.h"
#include <imgui.h>

void PlayerMovementController::Initialize(AnimationModel* model)
{
	animationModel_ = model;
	velocity_ = {};
	isGrounded_ = true;
}

void PlayerMovementController::Update(const PlayerController* controller, Camera* camera, float deltaTime, bool weaponReloading)
{
	Vector3 moveInput = controller->GetMoveInput();

	if (isAimingInput_ || weaponReloading)
	{
		isDashing_ = false;
		moveInput *= 0.5f; // ADSやリロード中は移動速度を半分にする
	}
	else if (isDashing_ && (moveInput.x != 0.0f || moveInput.z != 0.0f) && *stamina_ >= staminaDashCost_ * deltaTime)
	{
		isDashing_ = true;
		*stamina_ -= staminaDashCost_ * deltaTime;
		staminaRecoverTimer_ = 0.0f;
		isStaminaRecoverBlocked_ = true;
	}
	else
	{
		isDashing_ = false;
	}

	// カメラYawを使って移動方向変換
	float yaw = 0.0f;
	if (camera)
	{
		yaw = camera->GetRotate().y;
		float sinY = sinf(yaw);
		float cosY = cosf(yaw);
		moveInput = {
			moveInput.x * cosY - moveInput.z * sinY,
			0.0f,
			moveInput.x * sinY + moveInput.z * cosY
		};
	}

	animationModel_->SetRotate({ 0.0f, yaw, 0.0f });

	bool isAimingNow = (!weaponReloading && isAimingInput_);

	float speed = baseSpeed_;
	if (isDashing_) speed *= dashMultiplier_;
	if (isAimingNow)  speed *= adsSpeedFactor_; camera->SetFovY(isAimingNow ? 0.5f : 1.0f); // ADS時のFOV調整
	if (isCrouchInput_) speed *= crouchSpeedFactor_;

	Vector3 pos = animationModel_->GetTranslate();
	pos += moveInput * speed * deltaTime;

	// ジャンプ
	if (isGrounded_ && controller->IsJumpTriggered())
	{
		velocity_.y = jumpPower_;
		isGrounded_ = false;
		*stamina_ -= staminaJumpCost_;
		isStaminaRecoverBlocked_ = true;
		staminaRecoverTimer_ = 0.0f;
	}

	velocity_.y += gravity_ * deltaTime;
	pos.y += velocity_.y;

	if (pos.y <= 0.0f)
	{
		pos.y = 0.0f;
		velocity_.y = 0.0f;
		isGrounded_ = true;
	}

	animationModel_->SetTranslate(pos);

	// スタミナの更新
	UpdateStamina(deltaTime);
}

void PlayerMovementController::DrawImGui()
{
	ImGui::Separator();
	ImGui::Text("== Stamina Info ==");
	ImGui::Text("Position: (%.2f, %.2f, %.2f)", animationModel_->GetTranslate().x, animationModel_->GetTranslate().y, animationModel_->GetTranslate().z);
	ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity_.x, velocity_.y, velocity_.z);
	ImGui::Text("Is Grounded: %s", isGrounded_ ? "Yes" : "No");
	ImGui::Text("Is Dashing: %s", isDashing_ ? "Yes" : "No");
	ImGui::Text("Stamina: %.2f / %.2f", *stamina_, maxStamina_);
}

void PlayerMovementController::UpdateStamina(float deltaTime)
{
	// スタミナ回復ブロック中の経過時間を計測
	if (isStaminaRecoverBlocked_)
	{
		staminaRecoverTimer_ += deltaTime;

		// 一定時間経過で回復可能に
		if (staminaRecoverTimer_ >= staminaRecoverDelay_)
		{
			isStaminaRecoverBlocked_ = false;
			staminaRecoverTimer_ = 0.0f;
		}
	}

	// 回復が許可されていればスタミナを回復
	if (!isStaminaRecoverBlocked_)
	{
		*stamina_ += staminaRegenRate_ * deltaTime;
		if (*stamina_ > maxStamina_)
		{
			*stamina_ = maxStamina_;
		}
	}
}
