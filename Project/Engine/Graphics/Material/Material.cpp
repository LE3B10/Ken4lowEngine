#include "Material.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	static_assert(sizeof(Material::MaterialCBData) == 144, "MaterialCBData must match the HLSL b0 layout.");

	namespace
	{
		constexpr const char* kNeutralTextureKey = "Generated/Material/neutral_white.dds";

		D3D12_GPU_DESCRIPTOR_HANDLE LoadTextureHandle(const std::string& texturePath)
		{
			TextureManager* textureManager = TextureManager::GetInstance();
			textureManager->LoadTexture(texturePath);
			return textureManager->GetSrvHandleGPU(texturePath);
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetNeutralTextureHandle()
		{
			TextureManager* textureManager = TextureManager::GetInstance();
			textureManager->CreateSolidColorTexture(kNeutralTextureKey, 255, 255, 255, 255, 1, 1);
			return textureManager->GetSrvHandleGPU(kNeutralTextureKey);
		}
	}

	void MaterialTextureSlots::ApplyDesc(const MaterialDesc& desc)
	{
		Reset();

		const std::string& baseColorPath = desc.preferPbrWorkflow
			? desc.pbr.baseColorTexturePath
			: desc.legacy.baseColorTexturePath;
		if (!baseColorPath.empty())
		{
			baseColor_ = LoadTextureHandle(baseColorPath);
			hasBaseColorOverride_ = true;
		}

		if (!desc.preferPbrWorkflow)
		{
			return; // Legacy Materialは従来どおりBaseColorだけを使用する。
		}

		if (!desc.pbr.metallicRoughnessTexturePath.empty())
		{
			metallicRoughness_ = LoadTextureHandle(desc.pbr.metallicRoughnessTexturePath);
		}
		if (!desc.pbr.normalTexturePath.empty())
		{
			normal_ = LoadTextureHandle(desc.pbr.normalTexturePath);
		}
		if (!desc.pbr.occlusionTexturePath.empty())
		{
			occlusion_ = LoadTextureHandle(desc.pbr.occlusionTexturePath);
		}
		if (!desc.pbr.emissiveTexturePath.empty())
		{
			emissive_ = LoadTextureHandle(desc.pbr.emissiveTexturePath);
		}
	}

	void MaterialTextureSlots::Reset()
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE neutral = GetNeutralTextureHandle();
		baseColor_ = {};
		metallicRoughness_ = neutral;
		normal_ = neutral;
		occlusion_ = neutral;
		emissive_ = neutral;
		hasBaseColorOverride_ = false;
	}

	void MaterialTextureSlots::Clear()
	{
		baseColor_ = {};
		metallicRoughness_ = {};
		normal_ = {};
		occlusion_ = {};
		emissive_ = {};
		hasBaseColorOverride_ = false;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE MaterialTextureSlots::ResolveBaseColor(D3D12_GPU_DESCRIPTOR_HANDLE modelBaseColor) const
	{
		return hasBaseColorOverride_ ? baseColor_ : modelBaseColor;
	}

	void MaterialTextureSlots::BindAdditionalSlots(
		ID3D12GraphicsCommandList* commandList,
		UINT metallicRoughnessRootIndex,
		UINT normalRootIndex,
		UINT occlusionRootIndex,
		UINT emissiveRootIndex) const
	{
		if (!commandList)
		{
			return;
		}

		commandList->SetGraphicsRootDescriptorTable(metallicRoughnessRootIndex, metallicRoughness_);
		commandList->SetGraphicsRootDescriptorTable(normalRootIndex, normal_);
		commandList->SetGraphicsRootDescriptorTable(occlusionRootIndex, occlusion_);
		commandList->SetGraphicsRootDescriptorTable(emissiveRootIndex, emissive_);
	}

	/// -------------------------------------------------------------
	///				　		 初期化処理
	/// -------------------------------------------------------------
	void Material::Initialize()
	{
		ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

		// マテリアル用定数バッファを 1 つ分確保して、CPU から書き込めるようにマップする
		materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialCBData));
		materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

		ResetToDefault(); // 初期化とMaterial Binding解除で同じ既定値を使用する。
	}

	void Material::ResetToDefault()
	{
		if (!materialData_)
		{
			return;
		}

		// 既存Forward描画と同じ値へ戻し、Material未指定Actorの見た目を維持する。
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
		materialData_->emissiveFactor = { 0.0f, 0.0f, 0.0f, 1.0f };
		materialData_->textureFlags = 0;
		materialData_->padding[0] = materialData_->padding[1] = materialData_->padding[2] = 0.0f;
	}

	void Material::ApplyDesc(const MaterialDesc& desc)
	{
		if (!materialData_)
		{
			return;
		}

		const bool usePbr = desc.preferPbrWorkflow;
		materialData_->color = usePbr ? desc.pbr.baseColorFactor : desc.legacy.color;
		materialData_->shininess = desc.legacy.shininess;
		materialData_->pbrEnabled = usePbr ? 1.0f : 0.0f;
		materialData_->metallic = desc.pbr.metallicFactor;
		materialData_->normalScale = desc.pbr.normalScale;
		materialData_->uvTransform = desc.legacy.uvTransform;
		materialData_->reflection = desc.legacy.reflection;
		materialData_->roughness = usePbr ? desc.pbr.roughnessFactor : desc.legacy.roughness;
		materialData_->usePointSampling = desc.legacy.usePointSampling ? 1.0f : 0.0f;
		materialData_->occlusionStrength = desc.pbr.occlusionStrength;
		materialData_->emissiveFactor = desc.pbr.emissiveFactor;
		materialData_->textureFlags = 0;
		if (usePbr && !desc.pbr.metallicRoughnessTexturePath.empty()) { materialData_->textureFlags |= 1u << 0; }
		if (usePbr && !desc.pbr.normalTexturePath.empty()) { materialData_->textureFlags |= 1u << 1; }
		if (usePbr && !desc.pbr.occlusionTexturePath.empty()) { materialData_->textureFlags |= 1u << 2; }
		if (usePbr && !desc.pbr.emissiveTexturePath.empty()) { materialData_->textureFlags |= 1u << 3; }
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
			materialData_->emissiveFactor = this->materialData_->emissiveFactor;
			materialData_->textureFlags = this->materialData_->textureFlags;
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
			ImGui::ColorEdit4("Emissive##Material", &materialData_->emissiveFactor.x);
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
