#include "FpsCamera.h"
#include <Matrix4x4.h>
#include <Object3DCommon.h>
#include "Player.h"
#include <Camera.h>
#include <Input.h>


void FpsCamera::Initialize(Player* player)
{
	input_ = Input::GetInstance();
	player_ = player;
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
}

void FpsCamera::Update(bool ignoreInput)
{
	if (!player_ || !camera_) return;

	if (!ignoreInput)
	{
		int mouseDX = input_->GetMouseMoveX();
		int mouseDY = input_->GetMouseMoveY();

		// --- ADS状態によって感度を変更 ---
		float effectiveMouseSensitivity = isAiming_ ? mouseSensitivity_ * 0.5f : mouseSensitivity_;
		float effectiveControllerSensitivity = isAiming_ ? controllerSensitivity_ * 0.5f : controllerSensitivity_;

		float sensitivityModifier = player_->IsAiming() ? adsSensitivityFactor_ : 1.0f;

		float deltaYaw = -mouseDX * mouseSensitivity_ * sensitivityModifier;
		float deltaPitch = mouseDY * mouseSensitivity_ * sensitivityModifier;

		Vector2 rStick = input_->GetRightStick();
		if (!input_->RStickInDeadZone()) {
			deltaYaw += -rStick.x * controllerSensitivity_ * sensitivityModifier;
			deltaPitch += -rStick.y * controllerSensitivity_ * sensitivityModifier;
		}

		yaw_ += deltaYaw;
		pitch_ = std::clamp(pitch_ + deltaPitch, minPitch_, maxPitch_);
	}

	// --- カメラ位置設定（プレイヤーの頭位置） ---
	Vector3 playerPos = player_->GetWorldPosition();
	Vector3 camPos = playerPos + Vector3{ 0, eyeHeight_, 0 };

	// --- オイラー角ベースのビュー行列作成 ---
	Vector3 camEuler = { pitch_, yaw_, 0.0f }; // X, Y 回転
	Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(camEuler);
	Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(camPos);
	Matrix4x4 viewMat = Matrix4x4::Inverse(rotMat * transMat);

	camera_->SetViewMatrix(viewMat);
	camera_->SetTranslate(camPos);
	camera_->SetRotate(camEuler);
}
