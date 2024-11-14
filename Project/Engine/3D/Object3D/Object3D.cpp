#include "Object3D.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"
#include "MatrixMath.h"
#include "ResourceManager.h"

#include <assert.h>
#include "ModelManager.h"
#include "Model.h"


/// -------------------------------------------------------------
///					　		初期化処理
/// -------------------------------------------------------------
void Object3D::Initialize(const std::string& fileName)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	model_ = std::make_unique<Model>();
	model_->Initialize("Resources", fileName);

	transform.scale = { 1.0f,1.0f,1.0f };
	transform.rotate = { 0.0f, 0.0f, 0.0f };
	transform.translate = { 0.0f, 0.0f, 0.0f };

	model_->SetRotate(transform.rotate);
	model_->SetTranslate(transform.translate);
}


/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void Object3D::Update()
{
	model_->SetTranslate(transform.translate);
	model_->SetRotate(transform.rotate);
	model_->SetScale(transform.scale);

	model_->Update();
}


/// -------------------------------------------------------------
///					　		ImGuiの描画
/// -------------------------------------------------------------
void Object3D::DrawImGui()
{
	if (ImGui::TreeNode("Transform"))
	{
		ImGui::DragFloat3("Position", &transform.translate.x, 0.01f); // 座標変更
		ImGui::DragFloat3("Rotation", &transform.rotate.x, 0.01f);   // 回転変更
		ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);       // スケール変更
		ImGui::TreePop();
	}
}

void Object3D::CameraImGui()
{
	model_->CameraImGui();
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void Object3D::DrawCall(ID3D12GraphicsCommandList* commandList)
{
	model_->DrawCall(commandList);
}


/// -------------------------------------------------------------
///					　頂点バッファの設定
/// -------------------------------------------------------------
void Object3D::SetObject3DBufferData(ID3D12GraphicsCommandList* commandList)
{
	model_->SetBufferData(commandList);
}


/// -------------------------------------------------------------
///					　モデルの設定
/// -------------------------------------------------------------
void Object3D::SetModel(const std::string& filePath)
{
	// モデルを検索してセットする (例: 方法1)
	model_ = std::move(ModelManager::GetInstance()->FindModel(filePath));

	// モデルがセットされた後に初期化が必要な場合
	if (model_) {
		model_->Initialize("Resources", filePath);
	}
}
