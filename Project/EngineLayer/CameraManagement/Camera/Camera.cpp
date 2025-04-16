#include "Camera.h"
#include "ImGuiManager.h"
#include <WinApp.h>
#include <ParameterManager.h>
#include "Matrix4x4.h"

Camera::Camera() :
	fovY_(0.45f),
	aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight)),
	nearClip_(0.1f), farClip_(100.0f),
	worldMatrix_(Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_)),
	viewMatrix_(Matrix4x4::Inverse(worldMatrix_)),
	projectionMatrix_(Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_)),
	viewProjectionMatrix_(Matrix4x4::Multiply(viewMatrix_, projectionMatrix_))
{
	worldTransform_.Initialize();
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Camera::Update()
{
	// ビュー行列の計算処理
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

	// プロジェクション行列の更新
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::DrawImGui()
{
	ImGui::Begin("Camera");
	ImGui::DragFloat3("Position", &worldTransform_.translate_.x, 0.1f);
	ImGui::DragFloat3("Rotation", &worldTransform_.rotate_.x, 0.01f);
	ImGui::End();
}
