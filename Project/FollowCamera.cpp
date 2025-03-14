#include "FollowCamera.h"
#include <Object3DCommon.h>
#include <Input.h>
#include <Player.h>
#include <Enemy.h>
#include <LockOn.h>


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

	// **ロックオン中かチェック**
	if (lockOn_ && lockOn_->GetIsLockOn() && lockOn_->ExistTarget())
	{
		// ロックオン中は対象を向く
		LookAtLockOnTarget();
	}
	else
	{
		// **通常のカメラ回転処理**
		HandleCameraRotation();
	}

	// **ターゲットの位置を補間**
	Vector3 targetPos = target_->worldTranslate_;
	interTarget_ = Vector3::Lerp(interTarget_, targetPos, 0.1f);

	// **カメラのオフセットを計算**
	Vector3 newCameraPos = CalculateCameraPosition();

	// **補間を適用**
	ApplyCameraTransform(newCameraPos);
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

Vector3 FollowCamera::UpdateOffset()
{
	// **プレイヤーがダッシュ中か判定**
	bool isDashing = (player_ && player_->IsDashing());

	Vector3 dashOffset = { 0.0f, 12.0f, -40.0f };
	Vector3 normalOffset = { 0.0f, 10.0f, -35.0f };

	// **ゲームパッドのLトリガーを押している間はカメラを近づける**
	Vector3 closeOffset = { 0.0f, 8.0f, -25.0f };
	bool isCloseView = input_->PushButton(XButtons.L_Trigger);

	return Vector3::Lerp(offset_, isCloseView ? closeOffset : (isDashing ? dashOffset : normalOffset), 0.1f);
}

void FollowCamera::HandleCameraRotation()
{
	float targetYaw = cameraYaw_;
	float targetPitch = cameraPitch_;

	// **Shiftキーで回転速度を上げる**
	float speedMultiplier = (input_->PushKey(DIK_LSHIFT)) ? 2.0f : 1.0f;

	// **キーボード操作**
	if (input_->PushKey(DIK_LEFT)) { targetYaw += rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_RIGHT)) { targetYaw -= rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_UP)) { targetPitch -= rotationSpeed_ * speedMultiplier; }
	if (input_->PushKey(DIK_DOWN)) { targetPitch += rotationSpeed_ * speedMultiplier; }

	// **ゲームパッド操作**
	Vector2 rightStick = input_->GetRightStick();
	if (!input_->RStickInDeadZone())
	{
		targetYaw -= rightStick.x * rotationSpeed_ * 2.0f;
		targetPitch -= rightStick.y * rotationSpeed_ * 2.0f;
	}

	// **上下回転の制限**
	targetPitch = std::clamp(targetPitch, minPitch_, maxPitch_);

	// **Rキー or ゲームパッドのRスティック押し込みでカメラリセット**
	if ((input_->PushKey(DIK_R) || input_->TriggerButton(XButtons.R_Thumbstick)) && player_)
	{
		targetYaw = player_->GetYaw();
		targetPitch = 0.3f;
	}

	// **スムーズに補間**
	cameraYaw_ = std::lerp(cameraYaw_, targetYaw, 0.3f);
	cameraPitch_ = std::lerp(cameraPitch_, targetPitch, 0.3f);
}

Vector3 FollowCamera::CalculateCameraPosition()
{
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

	return interTarget_ + rotatedOffset;
}

void FollowCamera::ApplyCameraTransform(const Vector3& newCameraPos)
{
	// **カメラの位置を補間**
	camera_->SetTranslate(Vector3::Lerp(camera_->GetTranslate(), newCameraPos, 0.1f));

	// **カメラの回転を補間**
	Vector3 newRotation = { cameraPitch_, cameraYaw_, 0.0f };
	camera_->SetRotate(Vector3::Lerp(camera_->GetRotate(), newRotation, 0.1f));

	// **カメラの更新**
	camera_->Update();
}

void FollowCamera::LookAtLockOnTarget()
{
	constexpr float PI = std::numbers::pi_v<float>;
	constexpr float TWO_PI = std::numbers::pi_v<float> *2.0f;

	// **ロックオン対象の座標を取得**
	Vector3 lockOnPosition = lockOn_->GetTargetPosition();
	Vector3 cameraPos = camera_->GetTranslate();

	// **カメラからロックオン対象への方向ベクトルを計算**
	Vector3 toTarget = Vector3::Normalize(lockOnPosition - cameraPos);

	// **Yaw（左右回転）を計算**
	float targetYaw = std::atan2(-toTarget.x, toTarget.z);

	// **Pitch（上下回転）を計算**
	float distanceXZ = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
	float targetPitch = std::atan2(toTarget.y + 0.55f, distanceXZ);

	// **上下回転の制限**
	targetPitch = std::clamp(targetPitch, minPitch_, maxPitch_);

	// **Yawの角度差を補正（-π〜πの範囲にする）**
	float deltaYaw = targetYaw - cameraYaw_;
	if (deltaYaw > PI) deltaYaw -= TWO_PI;
	if (deltaYaw < -PI) deltaYaw += TWO_PI;
	targetYaw = cameraYaw_ + deltaYaw;

	// **スムーズにカメラの回転を補間（補間速度を調整）**
	cameraYaw_ = std::lerp(cameraYaw_, targetYaw, 0.1f);
	cameraPitch_ = std::lerp(cameraPitch_, targetPitch, 0.1f);
}
