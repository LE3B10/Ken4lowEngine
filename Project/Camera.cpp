#include "Camera.h"
#include "MatrixMath.h"


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
	// ビュー行列の計算処理
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	viewMatrix = Inverse(worldMatrix);

	// プロジェクション行列の更新
	projectionMatrix = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	viewProjevtionMatrix = Multiply(viewMatrix, projectionMatrix);
}
