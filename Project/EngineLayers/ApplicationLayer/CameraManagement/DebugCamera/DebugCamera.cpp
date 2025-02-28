#include "DebugCamera.h"
#include "WinApp.h"
#include "Input.h"
#include "ParameterManager.h"

/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
DebugCamera* DebugCamera::GetInstance()
{
	static DebugCamera instance;
	return &instance;
}


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugCamera::Initialize()
{
	worldTransform_.Initialize();
	worldTransform_.translation_ = { 0.0f,0.0f,-50.0f };

	fovY_ = 0.45f;
	aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
	nearClip_ = 0.1f;
	farClip_ = 100.0f;
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translation_);
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);

	rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
}


/// -------------------------------------------------------------
///							　更新処理
/// -------------------------------------------------------------
void DebugCamera::Update()
{
	Move();

	// **回転行列を更新**
	rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);

	// ワールド行列を作る
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translation_);

	// ビュー行列を作る
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

	// ビュー射影行列を更新
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
}


/// -------------------------------------------------------------
///							移動操作処理
/// -------------------------------------------------------------
void DebugCamera::Move()
{
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	// カメラの前後左右移動
	if (Input::GetInstance()->PushKey(DIK_W)) { move.z += 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_S)) { move.z -= 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_A)) { move.x -= 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_D)) { move.x += 0.4f; }

	// カメラの高さ移動
	if (Input::GetInstance()->PushKey(DIK_SPACE)) { move.y += 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_LSHIFT)) { move.y -= 0.4f; }

	// **回転行列を適用して移動方向を修正**
	move = Vector3::Transform(move, rotateMatrix_);

	// 計算した移動量を適用
	worldTransform_.translation_ += move;

	// **カメラの角度変更**
	if (Input::GetInstance()->PushKey(DIK_UP)) { worldTransform_.rotate_.x -= 0.04f; }
	if (Input::GetInstance()->PushKey(DIK_DOWN)) { worldTransform_.rotate_.x += 0.04f; }
	if (Input::GetInstance()->PushKey(DIK_LEFT)) { worldTransform_.rotate_.y += 0.04f; }
	if (Input::GetInstance()->PushKey(DIK_RIGHT)) { worldTransform_.rotate_.y -= 0.04f; }
}
