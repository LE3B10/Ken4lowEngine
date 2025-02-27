#include "FollowCamera.h"
#include <Object3DCommon.h>
#include <Camera.h>
#include <Input.h>


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

	// **カメラの回転処理（左右キー）**
	if (input_->PushKey(DIK_LEFT)) { cameraYaw_ += rotationSpeed_; } // 左回転
	if (input_->PushKey(DIK_RIGHT)) { cameraYaw_ -= rotationSpeed_; } // 右回転

	// **カメラの上下回転処理（Pitch: 上下キー）**
	if (input_->PushKey(DIK_UP)) { cameraPitch_ -= rotationSpeed_; }// 上回転
	if (input_->PushKey(DIK_DOWN)) { cameraPitch_ += rotationSpeed_; }// 下回転

	// **X軸回転（上下）のデッドゾーン適用**
	if (cameraPitch_ > maxPitch_) cameraPitch_ = maxPitch_;
	if (cameraPitch_ < minPitch_) cameraPitch_ = minPitch_;


	// **ターゲット（プレイヤー）のワールド行列を更新**
	const_cast<WorldTransform*>(target_)->Update();

	// **ターゲットのワールド座標を取得**
	Vector3 targetPos = target_->worldTranslate_;

	// **カメラのオフセットを回転**
	float sinYaw = sinf(cameraYaw_);
	float cosYaw = cosf(cameraYaw_);
	float sinPitch = sinf(cameraPitch_);
	float cosPitch = cosf(cameraPitch_);

	// **オフセットを Yaw & Pitch で回転**
	Vector3 rotatedOffset = {
		offset_.x * cosYaw - offset_.z * sinYaw,
		offset_.y * cosPitch - offset_.z * sinPitch,
		offset_.x * sinYaw + offset_.z * cosYaw
	};

	// **カメラの位置を計算**
	Vector3 newCameraPos = targetPos + rotatedOffset;

	// **スムーズにカメラを移動**
	camera_->SetTranslate(Vector3::Lerp(camera_->GetTranslate(), newCameraPos, 0.1f));

	// **カメラの回転を設定**
	Vector3 newRotation = { cameraPitch_, cameraYaw_, 0.0f };
	camera_->SetRotate(Vector3::Lerp(camera_->GetRotate(), newRotation, 0.1f));

	// **カメラの更新**
	camera_->Update();
}
