#include "MainCamera3D.h"
#include "MatrixMath.h"
#include "ImGuiManager.h"


/// -------------------------------------------------------------
///					シングルトンインスタンス
/// -------------------------------------------------------------
MainCamera3D* MainCamera3D::GetInstance()
{
	static MainCamera3D instance;
	return &instance;
}


/// -------------------------------------------------------------
///						初期化処理
/// -------------------------------------------------------------
void MainCamera3D::Initialize()
{
	// トランスフォームの初期化
	worldTransform_ = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f, 0.0f} };

	// アフィン変換
	worldMatrix_ = MakeAffineMatrix(worldTransform_.scale, worldTransform_.rotate, worldTransform_.translate);

	// 透視投影行列
	projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
}


/// -------------------------------------------------------------
///						更新処理
/// -------------------------------------------------------------
void MainCamera3D::Update()
{
	// アフィン変換
	worldMatrix_ = MakeAffineMatrix(worldTransform_.scale, worldTransform_.rotate, worldTransform_.translate);
	// 逆行列
	viewMatirx_ = Inverse(worldMatrix_);
}


/// -------------------------------------------------------------
///					ImGui描画処理
/// -------------------------------------------------------------
void MainCamera3D::DrawImGui()
{
	ImGui::Begin("3DCamera");
	ImGui::DragFloat3("Position", &worldTransform_.translate.x, 0.01f);
	ImGui::SliderAngle("RotateX", &worldTransform_.rotate.x);
	ImGui::SliderAngle("RotateY", &worldTransform_.rotate.y);
	ImGui::SliderAngle("RotateZ", &worldTransform_.rotate.z);
	ImGui::End();
}
