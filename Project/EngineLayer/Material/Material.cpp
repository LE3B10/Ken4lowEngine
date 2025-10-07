#include "Material.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"

#include <imgui.h>


/// -------------------------------------------------------------
///				　		 初期化処理
/// -------------------------------------------------------------
void Material::Initialize()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialCBData));
	// 書き込むためのアドレスを取得
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 色
	materialData_->shininess = 32.0f;			   // シェーディングの強さ
	materialData_->reflection = 0.0f; 				   // 反射率
	materialData_->uvTransform = Matrix4x4::MakeIdentity(); // UV変換行列
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void Material::Update()
{
	if (materialData_)
	{
		materialData_->color = this->materialData_->color;					 // 色
		materialData_->shininess = this->materialData_->shininess;		 // シェーディングの強さ
		materialData_->reflection = this->materialData_->reflection;			 // シェーディングの強さ
		materialData_->uvTransform = this->materialData_->uvTransform;		 // UV変換行列
	}
}


/// -------------------------------------------------------------
///				　			パイプラインの設定
/// -------------------------------------------------------------
void Material::SetPipeline(UINT rootParameterIndex) const
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

	if (materialResource_)
	{
		commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, materialResource_->GetGPUVirtualAddress());
	}
}


/// -------------------------------------------------------------
///				　			ImGuiの描画
/// -------------------------------------------------------------
void Material::DrawImGui()
{
	if (ImGui::CollapsingHeader("Material Settings"))
	{
		ImGui::ColorEdit4("Color", &materialData_->color.x); // 色変更
		ImGui::DragFloat("Shininess", &materialData_->shininess, 1.0f, 1.0f, 256.0f); // シェーディングの強さ変更
		ImGui::DragFloat("Reflectivity", &materialData_->reflection, 0.01f, 0.0f, 1.0f); // シェーディングの強さ変更
	}
}
