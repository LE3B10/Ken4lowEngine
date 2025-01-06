#include "Camera.h"
#include "MatrixMath.h"
#include "ImGuiManager.h"


Camera::Camera() :
	transform({ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f,0.0f,0.0f } }),
	fovY_(0.45f), aspectRatio_(1280.0f / 720.0f), nearClip_(0.1f), farClip_(100.0f),
	worldMatrix(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate)),
	viewMatrix(Inverse(worldMatrix)),
	projectionMatrix(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_)),
	viewProjevtionMatrix(Multiply(viewMatrix, projectionMatrix))
{}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Camera::Update()
{
	float angleRadians = DegreesToRadians(-30.0f); // 30度をラジアンに変換

	if (followPlayer) {
		float height = 15.0f; // 高さ
		float distance = 20.0f; // 距離

		// 線形補間で追従
		transform.translate.x += (targetPosition_.x - transform.translate.x) * followSpeed_;
		transform.translate.y += (targetPosition_.y + height - transform.translate.y) * followSpeed_;
		transform.translate.z += (targetPosition_.z - distance - transform.translate.z) * followSpeed_;

		// 見下ろし角度を設定
		transform.rotate.x = -angleRadians;
		transform.rotate.y = 0.0f;
		transform.rotate.z = 0.0f;
	}

	// ビュー行列の計算処理
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	viewMatrix = Inverse(worldMatrix);

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

	// 追従設定
	ImGui::Checkbox("Follow Player", &followPlayer);

	if (followPlayer) {
		// 追従中の設定
		ImGui::SliderFloat("Follow Speed", &followSpeed_, 0.01f, 1.0f);
	}
	else {
		// 手動操作
		ImGui::DragFloat3("cameraTranslate", &transform.translate.x, 0.01f);
		ImGui::SliderAngle("CameraRotateX", &transform.rotate.x);
		ImGui::SliderAngle("CameraRotateY", &transform.rotate.y);
		ImGui::SliderAngle("CameraRotateZ", &transform.rotate.z);
	}

	ImGui::End();
}
