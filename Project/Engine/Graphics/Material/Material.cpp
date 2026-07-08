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
		materialData_->pbrEnabled = 0.0f;							 // 既存Legacy描画を初期状態として維持する
		materialData_->metallic = 0.0f;								 // Metallic/Roughness Texture未接続時は非金属へfallback
		materialData_->normalScale = 1.0f;							 // NormalMap未設定時は頂点法線をそのまま使う
		materialData_->reflection = 0.0f;							 // 反射なし
		materialData_->uvTransform = Matrix4x4::MakeIdentity();		 // UV はそのまま
		materialData_->roughness = 0.5f;							 // 中程度の粗さ
		materialData_->usePointSampling = 0.0f;					 // 既定は従来どおり Linear
		materialData_->occlusionStrength = 1.0f;					 // AO Texture未接続時も暗くなりすぎないfallback
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
			materialData_->pbrEnabled = this->materialData_->pbrEnabled;		 // Legacy/PBR切り替え
			materialData_->metallic = this->materialData_->metallic;			 // PBR metallic fallback
			materialData_->normalScale = this->materialData_->normalScale;		 // NormalMap scale fallback
			materialData_->reflection = this->materialData_->reflection;			 // シェーディングの強さ
			materialData_->uvTransform = this->materialData_->uvTransform;		 // UV変換行列
			materialData_->roughness = this->materialData_->roughness;			 // 粗さ
			materialData_->usePointSampling = this->materialData_->usePointSampling;
			materialData_->occlusionStrength = this->materialData_->occlusionStrength;
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
			// PBRは既存Legacy描画と共存させ、Material単位で明示的にONにしたときだけCook-Torrance経路を使う。
			bool pbrEnabled = materialData_->pbrEnabled > 0.5f;
			if (ImGui::Checkbox("Use PBR##Material", &pbrEnabled))
			{
				materialData_->pbrEnabled = pbrEnabled ? 1.0f : 0.0f;
			}
			ImGui::DragFloat("Metallic##Material", &materialData_->metallic, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Normal Scale##Material", &materialData_->normalScale, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("AO Strength##Material", &materialData_->occlusionStrength, 0.01f, 0.0f, 1.0f);
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
