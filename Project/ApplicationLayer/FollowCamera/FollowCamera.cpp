#include "FollowCamera.h"
#include <Object3DCommon.h>
#include <Input.h>
#include <Player.h>


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void FollowCamera::Initialize()
{
	input_ = Input::GetInstance();

	// カメラの生成
	camera_ = Object3DCommon::GetInstance()->GetDefaultCamera();
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void FollowCamera::Update()
{
	if (!target_) return; // 追従対象が設定されていなければ何もしない

	float targetYaw = cameraYaw_;
	float targetPitch = cameraPitch_;

	// **Shiftキーで回転速度を上げる**
	float speedMultiplier = (input_->PushKey(DIK_LSHIFT)) ? 2.0f : 1.0f;

	// **カメラの回転処理**
	if (input_->PushKey(DIK_LEFT)) { targetYaw += rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_RIGHT)) { targetYaw -= rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_UP)) { targetPitch -= rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_DOWN)) { targetPitch += rotationSpeed_ * speedMultiplier; }

	//// --- マウスでカメラ回転 ---
	//const float mouseRotateSpeed = 0.002f;

	//int deltaX = input_->GetMouseMoveX();
	//int deltaY = input_->GetMouseMoveY();

	//// 画面中央固定時のみマウス回転（または常時でOKならifは不要）
	//if (Input::GetInstance()->IsCursorLocked())
	//{
	//	targetYaw -= static_cast<float>(deltaX) * -mouseRotateSpeed;
	//	targetPitch -= static_cast<float>(deltaY) * -mouseRotateSpeed;
	//}

	// **ゲームパッドの右スティックでカメラ操作**
	Vector2 rightStick = input_->GetRightStick();
	if (!input_->RStickInDeadZone())
	{
		targetYaw -= rightStick.x * rotationSpeed_ * 2.0f;  // 横方向
		targetPitch -= rightStick.y * rotationSpeed_ * 2.0f; // 縦方向
	}

	targetPitch = std::clamp(targetPitch, minPitch_, maxPitch_);

	// **Rキー or ゲームパッドのRスティック押し込みでカメラリセット**
	if ((input_->PushKey(DIK_R) || input_->TriggerButton(XButtons.R_Thumbstick)) && player_)
	{
		targetYaw = player_->GetYaw();
		targetPitch = 0.3f;
	}

	// **スムーズに角度を補間（補間速度を増やす）**
	cameraYaw_ = std::lerp(cameraYaw_, targetYaw, 0.3f);
	cameraPitch_ = std::lerp(cameraPitch_, targetPitch, 0.3f);

	// **ターゲットの位置を補間**
	Vector3 targetPos = target_->worldTranslate_;
	interTarget_ = Vector3::Lerp(interTarget_, targetPos, 0.1f);

	// **カメラのオフセットを回転**
	float sinYaw = sinf(cameraYaw_);
	float cosYaw = cosf(cameraYaw_);
	float sinPitch = sinf(cameraPitch_);
	float cosPitch = cosf(cameraPitch_);

	Vector3 rotatedOffset = {
		offset_.x * cosYaw - offset_.z * sinYaw,
		offset_.y * cosPitch - offset_.z * sinPitch,
		offset_.x * sinYaw + offset_.z * cosYaw
	};

	// **カメラの位置を計算**
	Vector3 newCameraPos = interTarget_ + rotatedOffset;

	// **カメラの補間速度を変更**
	camera_->SetTranslate(Vector3::Lerp(camera_->GetTranslate(), newCameraPos, 0.1f));

	// **カメラの回転を補間**
	Vector3 newRotation = { cameraPitch_, cameraYaw_, 0.0f };
	camera_->SetRotate(Vector3::Lerp(camera_->GetRotate(), newRotation, 0.1f));

	// **カメラの更新**
	camera_->Update();
}


/// -------------------------------------------------------------
///							リセット処理
/// -------------------------------------------------------------
void FollowCamera::Reset()
{
	// 追従対象がいれば
	if (target_)
	{
		interTarget_ = target_->worldTranslate_; // 現在の位置を初期値にする
	}
}

//Vector3 FollowCamera::UpdateOffset()
//{
//	// **プレイヤーがダッシュ中か判定**
//	bool isDashing = (player_ && player_->IsDashing());
//
//	Vector3 dashOffset = { 0.0f, 12.0f, -40.0f };
//	Vector3 normalOffset = { 0.0f, 10.0f, -35.0f };
//
//	// **ゲームパッドのLトリガーを押している間はカメラを近づける**
//	Vector3 closeOffset = { 0.0f, 8.0f, -25.0f };
//	bool isCloseView = input_->PushButton(XButtons.L_Trigger);
//
//	return Vector3::Lerp(offset_, isCloseView ? closeOffset : (isDashing ? dashOffset : normalOffset), 0.1f);
//}
