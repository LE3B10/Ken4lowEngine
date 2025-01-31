#include "Camera.h"
#include "MatrixMath.h"
#include "VectorMath.h"
#include "ImGuiManager.h"
#include <WinApp.h>
#include <ParameterManager.h>
#include "Input.h"

Camera::Camera() :
	worldTransform({ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f,0.0f,0.0f } }),
	fovY_(0.45f),
	aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight)),
	nearClip_(0.1f), farClip_(100.0f),
	worldMatrix(MakeAffineMatrix(worldTransform.scale, worldTransform.rotate, worldTransform.translate)),
	viewMatrix(Inverse(worldMatrix)),
	projectionMatrix(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_)),
	viewProjevtionMatrix(Multiply(viewMatrix, projectionMatrix))
{
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Camera::Update()
{
	Input* input = Input::GetInstance();
	const float rotationSpeed = 0.008f; // 回転速度
	const float smoothing = 0.1f;        // 角度の補間速度
	const float minPitch = -0.15f;  // 上方向の回転制限
	const float maxPitch = 1.2f;   // 下方向の回転制限
	const float cameraLagFactor = 0.01f; // カメラの追従遅延

	// **カメラの初期位置をプレイヤーの真後ろにする**
	if (yaw_ == 0.0f) {
		yaw_ = -PI / 2.0f; // 真後ろ（180度回転）
	}

	// 矢印キーでカメラを回転
	if (input->PushKey(DIK_LEFT)) {
		yaw_ += rotationSpeed;
	}
	if (input->PushKey(DIK_RIGHT)) {
		yaw_ -= rotationSpeed;
	}
	if (input->PushKey(DIK_UP)) {
		pitch_ -= rotationSpeed;
	}
	if (input->PushKey(DIK_DOWN)) {
		pitch_ += rotationSpeed;
	}

	// ピッチの制限（カメラが真上や真下に行かないようにする）
	pitch_ = std::clamp(pitch_, minPitch, maxPitch);

	// **カメラの位置をプレイヤーを中心に計算**
	Vector3 playerPosition = targetPosition_; // プレイヤーの位置

	// **目標位置 (`targetPosition_`) に向かって `Lerp` で追従**
	worldTransform.translate = Lerp(worldTransform.translate, targetPosition_, cameraLagFactor);

	// 球面座標系を使用してカメラ位置を決定
	worldTransform.translate.x = playerPosition.x + distance_ * cosf(yaw_) * cosf(pitch_);
	worldTransform.translate.y = playerPosition.y + distance_ * sinf(pitch_);
	worldTransform.translate.z = playerPosition.z + distance_ * sinf(yaw_) * cosf(pitch_);

	// **プレイヤーを常に注視する**
	Vector3 forward = Normalize(playerPosition - worldTransform.translate);  // カメラの前方向
	Vector3 right = Normalize(Cross(Vector3(0, 1, 0), forward));             // 右方向
	Vector3 up = Cross(forward, right);                                      // 上方向

	// **ビュー行列を更新**

	// F5キーの状態を取得
	bool isF5Pressed = input->PushKey(DIK_F5);

	// **キーが押された瞬間だけトグル**
	if (isF5Pressed && !prevF5Pressed_)
	{
		isViewChange_ = !isViewChange_; // 視点を切り替え
	}

	// **現在のキー状態を保存**
	prevF5Pressed_ = isF5Pressed;

	if (!isViewChange_)
	{
		viewMatrix = {
			right.x, up.x, forward.x, 0,
			right.y, up.y, forward.y, 0,
			right.z, up.z, forward.z, 0,
			-Dot(right, worldTransform.translate), -Dot(up, worldTransform.translate) - 8.0f, -Dot(forward, worldTransform.translate) - 8.0f, 1
		};
	}
	else
	{
		viewMatrix = {
			right.x, up.x, forward.x, 0,
			right.y, up.y, forward.y, 0,
			right.z, up.z, forward.z, 0,
			-Dot(right, worldTransform.translate), -Dot(up, worldTransform.translate) - 10.0f, -Dot(forward, worldTransform.translate) + 50.0f, 1
		};
	}

	// ビュー行列の計算処理
	/*worldMatrix = MakeAffineMatrix(worldTransform.scale, worldTransform.rotate, worldTransform.translate);
	viewMatrix = Inverse(worldMatrix);*/

	// プロジェクション行列の更新
	projectionMatrix = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjevtionMatrix = Multiply(viewMatrix, projectionMatrix);
}


/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void Camera::DrawImGui()
{
	ImGui::Begin("Camera");
	ImGui::DragFloat3("cameraTranslate", &worldTransform.translate.x, 0.01f);
	ImGui::SliderAngle("CameraRotateX", &worldTransform.rotate.x);
	ImGui::SliderAngle("CameraRotateY", &worldTransform.rotate.y);
	ImGui::SliderAngle("CameraRotateZ", &worldTransform.rotate.z);
	ImGui::End();
}

Vector3 Camera::GetForwardDirection() const
{
	// ビュー行列の3列目を取得
	return Vector3(-viewMatrix.m[0][2], -viewMatrix.m[1][2], -viewMatrix.m[2][2]);
}
