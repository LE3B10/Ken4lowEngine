#include "Camera.h"
#include "ImGuiManager.h"
#include <WinApp.h>
#include <ParameterManager.h>
#include "Matrix4x4.h"

Camera::Camera() :
	fovY_(0.45f),
	aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight)),
	nearClip_(0.1f), farClip_(100.0f),
	worldMatrix(Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_)),
	viewMatrix(Matrix4x4::Inverse(worldMatrix)),
	projectionMatrix(Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_)),
	viewProjevtionMatrix(Matrix4x4::Multiply(viewMatrix, projectionMatrix))
{
	worldTransform.Initialize();
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Camera::Update()
{
	// ビュー行列の計算処理
	worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
	viewMatrix = Matrix4x4::Inverse(worldMatrix);

	// プロジェクション行列の更新
	projectionMatrix = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjevtionMatrix = Matrix4x4::Multiply(viewMatrix, projectionMatrix);
}


/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void Camera::DrawImGui()
{
	ImGui::Begin("Camera");
	ImGui::DragFloat3("cameraTranslate", &worldTransform.translate_.x, 0.01f);
	ImGui::SliderAngle("CameraRotateX", &worldTransform.rotate_.x);
	ImGui::SliderAngle("CameraRotateY", &worldTransform.rotate_.y);
	ImGui::SliderAngle("CameraRotateZ", &worldTransform.rotate_.z);
	ImGui::End();
}
