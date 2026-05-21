#include "Material.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　		 初期化処理
	/// -------------------------------------------------------------
	void Material::Initialize()
	{
		ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

		// マテリアル用定数バッファを 1 つ分確保して、CPU から書き込めるようにマップする
		materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialCBData));
		materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

		// デフォルト値で初期化
		materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };			 // 白
		materialData_->shininess = 32.0f;							 // 光沢度
		materialData_->reflection = 0.0f;							 // 反射なし
		materialData_->uvTransform = Matrix4x4::MakeIdentity();		 // UV はそのまま
		materialData_->roughness = 0.5f;							 // 中程度の粗さ
		materialData_->usePointSampling = 0.0f;					 // 既定は従来どおり Linear
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
			materialData_->roughness = this->materialData_->roughness;			 // 粗さ
			materialData_->usePointSampling = this->materialData_->usePointSampling;
		}
	}


	/// -------------------------------------------------------------
	///				　			パイプラインの設定
	/// -------------------------------------------------------------
	void Material::SetPipeline(UINT rootParameterIndex) const
	{
		ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

		// 有効な定数バッファがある場合のみ、指定されたルートパラメータにバインドする
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
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("Material Settings"))
		{
			// ベースカラー
			ImGui::ColorEdit4("Color", &materialData_->color.x);
			// 光沢度（スペキュラの鋭さ）
			ImGui::DragFloat("Shininess", &materialData_->shininess, 1.0f, 1.0f, 256.0f);
			// 反射率
			ImGui::DragFloat("Reflectivity", &materialData_->reflection, 0.01f, 0.0f, 1.0f);
			// 粗さ
			ImGui::DragFloat("Roughness", &materialData_->roughness, 0.01f, 0.0f, 1.0f);
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
