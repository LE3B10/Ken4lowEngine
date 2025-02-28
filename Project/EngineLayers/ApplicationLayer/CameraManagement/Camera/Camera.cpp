#include "Camera.h"
#include "ImGuiManager.h"
#include <WinApp.h>
#include <ParameterManager.h>
#include "Matrix4x4.h"

Camera::Camera() :
	fovY_(0.6f),
	aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight)),
	nearClip_(0.3f), farClip_(1000.0f),
	worldMatrix_(Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translation_)),
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
	worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translation_);
	viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

	// プロジェクション行列の更新
	projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
}
