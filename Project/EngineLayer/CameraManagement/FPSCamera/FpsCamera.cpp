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

	// フェーズ時カメラを更新しない
	if (!ignoreInput)
	{
		// --- 入力取得 ---
		int mouseDX = input_->GetMouseMoveX();
		int mouseDY = input_->GetMouseMoveY();

		float deltaYaw = -mouseDX * mouseSensitivity_;
		float deltaPitch = -mouseDY * mouseSensitivity_;

		Vector2 rStick = input_->GetRightStick();
		if (!input_->RStickInDeadZone()) {
			deltaYaw += -rStick.x * controllerSensitivity_;
			deltaPitch += -rStick.y * controllerSensitivity_;
		}

		yaw_ += deltaYaw;
		pitch_ = std::clamp(pitch_ + deltaPitch, minPitch_, maxPitch_);
	}

	// --- 回転クォータニオン生成 ---
	Quaternion qYaw = Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 1.0f, 0.0f }, -yaw_);
	Quaternion qPitch = Quaternion::MakeRotateAxisAngleQuaternion({ 1.0f, 0.0f, 0.0f }, -pitch_);
	Quaternion rotation = Quaternion::Multiply(qYaw, qPitch);

	// --- カメラ位置設定（プレイヤーの頭位置を想定） ---
	Vector3 playerPos = player_->GetWorldPosition(); // 体中心位置
	Vector3 camPos = playerPos + Vector3{ 0, eyeHeight_, 0 };

	// --- 回転行列を生成 ---
	Matrix4x4 rotMat = Quaternion::MakeRotateMatrix(rotation);
	Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(camPos);
	Matrix4x4 viewMat = Matrix4x4::Inverse(rotMat * transMat);

	camera_->SetViewMatrix(viewMat);
	camera_->SetTranslate(camPos); // ImGuiや3D表示用
	camera_->SetRotate({ pitch_, yaw_, 0.0f }); // デバッグ表示用
}
