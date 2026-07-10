#include "MaterialBinding.h"

#include "JsonReadUtil.h"
#include "MaterialDescLoader.h"
#include "MaterialRepository.h"

#include <array>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kAssetIdKey = "AssetId";
		constexpr const char* kUseOverrideKey = "UseOverride";
		constexpr const char* kOverrideKey = "Override";
		constexpr const char* kPreferPbrWorkflowKey = "preferPbrWorkflow";
		constexpr const char* kLegacyKey = "legacy";
		constexpr const char* kPbrKey = "pbr";
		constexpr const char* kUvTransformKey = "uvTransform";

		nlohmann::json Vector4ToJson(const Vector4& value)
		{
			return nlohmann::json::array({ value.x, value.y, value.z, value.w });
		}

		nlohmann::json MatrixToJson(const Matrix4x4& value)
		{
			nlohmann::json json = nlohmann::json::array();
			for (size_t row = 0; row < 4; ++row)
			{
				for (size_t column = 0; column < 4; ++column)
				{
					json.push_back(value.m[row][column]);
				}
			}
			return json;
		}

		Matrix4x4 ReadMatrixOr(const nlohmann::json& json, const char* key, const Matrix4x4& fallback)
		{
			const auto it = json.find(key);
			if (it == json.end() || !it->is_array() || it->size() != 16)
			{
				return fallback;
			}

			Matrix4x4 result{};
			for (size_t index = 0; index < 16; ++index)
			{
				if (!(*it)[index].is_number())
				{
					return fallback;
				}
				result.m[index / 4][index % 4] = (*it)[index].get<float>();
			}
			return result;
		}

		nlohmann::json MaterialDescToJson(const MaterialDesc& source)
		{
			const MaterialDesc desc = MaterialDescLoader::NormalizeDesc(source);
			nlohmann::json json = nlohmann::json::object();
			json[kPreferPbrWorkflowKey] = desc.preferPbrWorkflow;
			json[kLegacyKey] = {
				{ "color", Vector4ToJson(desc.legacy.color) },
				{ "shininess", desc.legacy.shininess },
				{ "reflectionRate", desc.legacy.reflection },
				{ "roughness", desc.legacy.roughness },
				{ "baseColorTexture", desc.legacy.baseColorTexturePath },
				{ "usePointSampling", desc.legacy.usePointSampling },
				{ kUvTransformKey, MatrixToJson(desc.legacy.uvTransform) },
			};
			json[kPbrKey] = {
				{ "baseColor", Vector4ToJson(desc.pbr.baseColorFactor) },
				{ "metallic", desc.pbr.metallicFactor },
				{ "roughness", desc.pbr.roughnessFactor },
				{ "normalScale", desc.pbr.normalScale },
				{ "occlusionStrength", desc.pbr.occlusionStrength },
				{ "emissiveColor", Vector4ToJson(desc.pbr.emissiveFactor) },
				{ "baseColorTexture", desc.pbr.baseColorTexturePath },
				{ "metallicRoughnessTexture", desc.pbr.metallicRoughnessTexturePath },
				{ "normalTexture", desc.pbr.normalTexturePath },
				{ "occlusionTexture", desc.pbr.occlusionTexturePath },
				{ "emissiveTexture", desc.pbr.emissiveTexturePath },
			};
			return json;
		}

		MaterialDesc MaterialDescFromJson(const nlohmann::json& json)
		{
			MaterialDesc desc = MaterialDescLoader::CreateDefaultDesc();
			if (!json.is_object())
			{
				return desc;
			}

			desc.preferPbrWorkflow = JsonReadUtil::ReadBoolOr(json, kPreferPbrWorkflowKey, desc.preferPbrWorkflow);
			const nlohmann::json legacy = JsonReadUtil::ReadObjectOr(json, kLegacyKey, nlohmann::json::object());
			const nlohmann::json pbr = JsonReadUtil::ReadObjectOr(json, kPbrKey, nlohmann::json::object());

			desc.legacy.color = JsonReadUtil::ReadVector4Or(legacy, "color", desc.legacy.color);
			desc.legacy.shininess = JsonReadUtil::ReadFloatOr(legacy, "shininess", desc.legacy.shininess);
			desc.legacy.reflection = JsonReadUtil::ReadFloatOr(legacy, "reflectionRate", desc.legacy.reflection);
			desc.legacy.roughness = JsonReadUtil::ReadFloatOr(legacy, "roughness", desc.legacy.roughness);
			desc.legacy.baseColorTexturePath = JsonReadUtil::ReadStringOr(legacy, "baseColorTexture", desc.legacy.baseColorTexturePath);
			desc.legacy.usePointSampling = JsonReadUtil::ReadBoolOr(legacy, "usePointSampling", desc.legacy.usePointSampling);
			desc.legacy.uvTransform = ReadMatrixOr(legacy, kUvTransformKey, desc.legacy.uvTransform);

			desc.pbr.baseColorFactor = JsonReadUtil::ReadVector4Or(pbr, "baseColor", desc.pbr.baseColorFactor);
			desc.pbr.metallicFactor = JsonReadUtil::ReadFloatOr(pbr, "metallic", desc.pbr.metallicFactor);
			desc.pbr.roughnessFactor = JsonReadUtil::ReadFloatOr(pbr, "roughness", desc.pbr.roughnessFactor);
			desc.pbr.normalScale = JsonReadUtil::ReadFloatOr(pbr, "normalScale", desc.pbr.normalScale);
			desc.pbr.occlusionStrength = JsonReadUtil::ReadFloatOr(pbr, "occlusionStrength", desc.pbr.occlusionStrength);
			desc.pbr.emissiveFactor = JsonReadUtil::ReadVector4Or(pbr, "emissiveColor", desc.pbr.emissiveFactor);
			desc.pbr.baseColorTexturePath = JsonReadUtil::ReadStringOr(pbr, "baseColorTexture", desc.pbr.baseColorTexturePath);
			desc.pbr.metallicRoughnessTexturePath = JsonReadUtil::ReadStringOr(pbr, "metallicRoughnessTexture", desc.pbr.metallicRoughnessTexturePath);
			desc.pbr.normalTexturePath = JsonReadUtil::ReadStringOr(pbr, "normalTexture", desc.pbr.normalTexturePath);
			desc.pbr.occlusionTexturePath = JsonReadUtil::ReadStringOr(pbr, "occlusionTexture", desc.pbr.occlusionTexturePath);
			desc.pbr.emissiveTexturePath = JsonReadUtil::ReadStringOr(pbr, "emissiveTexture", desc.pbr.emissiveTexturePath);
			return MaterialDescLoader::NormalizeDesc(desc);
		}
	}

	void MaterialBinding::SetUseOverride(bool enabled)
	{
		if (enabled && !useOverride_)
		{
			MaterialDesc assetDesc{};
			overrideDesc_ = ResolveAsset(assetDesc) ? assetDesc : MaterialDescLoader::CreateDefaultDesc();
		}

		useOverride_ = enabled;
	}

	bool MaterialBinding::Resolve(MaterialDesc& outDesc) const
	{
		if (useOverride_)
		{
			outDesc = MaterialDescLoader::NormalizeDesc(overrideDesc_); // Component固有値はGPU転送前に安全な範囲へ丸める。
			return true;
		}

		return ResolveAsset(outDesc);
	}

	bool MaterialBinding::ResolveAsset(MaterialDesc& outDesc) const
	{
		if (assetId_.empty())
		{
			return false; // Asset未指定時はモデルが元から持つMaterialを維持する。
		}

		const std::shared_ptr<MaterialAsset> asset = MaterialRepository::GetInstance()->FindById(assetId_);
		if (!asset)
		{
			return false;
		}

		outDesc = MaterialDescLoader::NormalizeDesc(asset->GetDesc());
		return true;
	}

	nlohmann::json MaterialBinding::ToJson() const
	{
		nlohmann::json json = {
			{ kAssetIdKey, assetId_ },
			{ kUseOverrideKey, useOverride_ },
		};
		if (useOverride_)
		{
			json[kOverrideKey] = MaterialDescToJson(overrideDesc_); // Override有効時だけComponent固有値をActor JSONへ保存する。
		}
		return json;
	}

	void MaterialBinding::FromJson(const nlohmann::json& json)
	{
		assetId_.clear();
		useOverride_ = false;
		overrideDesc_ = MaterialDescLoader::CreateDefaultDesc();
		if (!json.is_object())
		{
			return;
		}

		assetId_ = JsonReadUtil::ReadStringOr(json, kAssetIdKey, assetId_);
		useOverride_ = JsonReadUtil::ReadBoolOr(json, kUseOverrideKey, false);
		const nlohmann::json overrideJson = JsonReadUtil::ReadObjectOr(json, kOverrideKey, nlohmann::json::object());
		overrideDesc_ = MaterialDescFromJson(overrideJson);
	}
}
