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
	worldTransform_.translate_ = { 0.0f,0.0f,-50.0f };

	fovY_ = 0.45f;
	aspectRatio_ = float(WinApp::kClientWidth) / float(WinApp::kClientHeight);
	nearClip_ = 0.1f;
	farClip_ = 100.0f;

	UpdateViewProjection();
}

/// -------------------------------------------------------------
///							　更新処理
/// -------------------------------------------------------------
void DebugCamera::Update()
{
	// 移動処理
	Move();

	// 行列の更新
	UpdateViewProjection();
}


/// -------------------------------------------------------------
///							移動操作処理
/// -------------------------------------------------------------
void DebugCamera::Move()
{
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	/// ---------- キー入力による移動 ---------- ///
	if (Input::GetInstance()->PushKey(DIK_W)) { move.z += 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_S)) { move.z -= 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_A)) { move.x -= 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_D)) { move.x += 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_SPACE)) { move.y += 0.4f; }
	if (Input::GetInstance()->PushKey(DIK_LSHIFT)) { move.y -= 0.4f; }

	/// ---------- ホイールで前後移動 ---------- ///
	move.z += Input::GetInstance()->GetMouseWheel() * 0.02f;

	// 回転行列を適用して移動方向を修正
	move = Vector3::Transform(move, rotateMatrix_);
	worldTransform_.translate_ += move;

	/// ---------- マウスでカメラ回転 ---------- ///
	const float rotateSpeed = 0.002f;
	worldTransform_.rotate_.x += Input::GetInstance()->GetMouseMoveY() * rotateSpeed;
	worldTransform_.rotate_.y += Input::GetInstance()->GetMouseMoveX() * -rotateSpeed;

	// カメラのピッチ制限（±90度）
	worldTransform_.rotate_.x = std::clamp(worldTransform_.rotate_.x, -1.57f, 1.57f);
}

/// -------------------------------------------------------------
///						行列の更新処理
/// -------------------------------------------------------------
void DebugCamera::UpdateViewProjection()
{
	// **回転行列を更新**
	rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);

	// ワールド行列を作る
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);

	// ビュー行列を作る
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

	// 射影行列を更新
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);

	// ビュー射影行列を更新
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
}
