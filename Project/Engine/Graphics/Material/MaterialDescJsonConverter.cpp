#include "MaterialDescJsonConverter.h"

namespace Ken4lowEngine
{
	namespace
	{
		nlohmann::json AsObjectOrEmpty(const nlohmann::json& value)
		{
			return value.is_object() ? value : nlohmann::json::object();
		}

		std::string ReadString(const nlohmann::json& json, const char* key, const std::string& fallback)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_string())
			{
				return it->get<std::string>();
			}
			return fallback;
		}

		bool ReadBool(const nlohmann::json& json, const char* key, bool fallback)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_boolean())
			{
				return it->get<bool>();
			}
			return fallback;
		}

		float ReadFloat(const nlohmann::json& json, const char* key, float fallback)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_number())
			{
				return it->get<float>();
			}
			return fallback;
		}

		Vector4 ReadVector4(const nlohmann::json& json, const char* key, const Vector4& fallback)
		{
			const auto it = json.find(key);
			if (it == json.end() || !it->is_array() || it->size() < 4)
			{
				return fallback;
			}

			Vector4 result = fallback;
			for (size_t i = 0; i < 4; ++i)
			{
				if (!(*it)[i].is_number())
				{
					return fallback;
				}
				result[static_cast<int>(i)] = (*it)[i].get<float>();
			}
			return result;
		}

		nlohmann::json ToJsonVector4(const Vector4& value)
		{
			return nlohmann::json::array({ value.x, value.y, value.z, value.w });
		}
	}

	MaterialDescSource MaterialDescJsonConverter::FromJson(const nlohmann::json& json)
	{
		const nlohmann::json root = AsObjectOrEmpty(json);
		const nlohmann::json legacy = AsObjectOrEmpty(root.value(Keys::Legacy, nlohmann::json::object()));
		const nlohmann::json pbr = AsObjectOrEmpty(root.value(Keys::Pbr, nlohmann::json::object()));

		MaterialDescSource source{};
		source.sourceKind = SourceKindFromString(ReadString(root, Keys::SourceKind, "json"));
		source.materialId = ReadString(root, Keys::MaterialId, source.materialId);
		source.materialName = ReadString(root, Keys::MaterialName, source.materialName);
		source.sourcePath = ReadString(root, Keys::SourcePath, source.sourcePath);
		source.preferPbrWorkflow = ReadBool(root, Keys::PreferPbrWorkflow, source.preferPbrWorkflow);

		// 欠損キーはSource既定値へフォールバックし、古いJsonや途中作成中のMaterialでも読み取りを失敗させない。
		source.legacyColor = ReadVector4(legacy, Keys::LegacyColor, source.legacyColor);
		source.legacyShininess = ReadFloat(legacy, Keys::LegacyShininess, source.legacyShininess);
		source.legacyReflectionRate = ReadFloat(legacy, Keys::LegacyReflectionRate, source.legacyReflectionRate);
		source.legacyRoughness = ReadFloat(legacy, Keys::LegacyRoughness, source.legacyRoughness);
		source.usePointSampling = ReadBool(legacy, Keys::LegacyUsePointSampling, source.usePointSampling);

		source.baseColorFactor = ReadVector4(pbr, Keys::PbrBaseColor, source.baseColorFactor);
		source.metallicFactor = ReadFloat(pbr, Keys::PbrMetallic, source.metallicFactor);
		source.roughnessFactor = ReadFloat(pbr, Keys::PbrRoughness, source.roughnessFactor);
		source.normalScale = ReadFloat(pbr, Keys::PbrNormalScale, source.normalScale);
		source.occlusionStrength = ReadFloat(pbr, Keys::PbrOcclusionStrength, source.occlusionStrength);
		source.emissiveFactor = ReadVector4(pbr, Keys::PbrEmissiveColor, source.emissiveFactor);
		const float emissiveStrength = ReadFloat(pbr, Keys::PbrEmissiveStrength, 1.0f);
		source.emissiveFactor.x *= emissiveStrength;
		source.emissiveFactor.y *= emissiveStrength;
		source.emissiveFactor.z *= emissiveStrength;

		// Legacy/PBRは同じbaseColorTextureを使うことがあるため、PBR側が空ならLegacy側の指定をSourceへ残す。
		source.baseColorTexturePath = ReadString(pbr, Keys::PbrBaseColorTexture, ReadString(legacy, Keys::LegacyBaseColorTexture, source.baseColorTexturePath));
		source.normalTexturePath = ReadString(pbr, Keys::PbrNormalTexture, source.normalTexturePath);
		source.metallicRoughnessTexturePath = ReadString(pbr, Keys::PbrMetallicRoughnessTexture, source.metallicRoughnessTexturePath);
		source.occlusionTexturePath = ReadString(pbr, Keys::PbrOcclusionTexture, source.occlusionTexturePath);
		source.emissiveTexturePath = ReadString(pbr, Keys::PbrEmissiveTexture, source.emissiveTexturePath);

		const auto slotsIt = root.find(Keys::TextureSlots);
		if (slotsIt != root.end() && slotsIt->is_array())
		{
			for (const auto& slotJson : *slotsIt)
			{
				if (!slotJson.is_object())
				{
					continue;
				}

				MaterialSourceTextureSlot slot{};
				slot.semantic = ReadString(slotJson, Keys::TextureSlotSemantic, "");
				slot.texturePath = ReadString(slotJson, Keys::TextureSlotPath, "");
				if (!slot.semantic.empty() || !slot.texturePath.empty())
				{
					// TextureManagerへは接続せず、Json上のslot情報をSourceへ移すだけにする。
					source.textureSlots.push_back(slot);
				}
			}
		}

		return MaterialDescLoader::NormalizeSource(source);
	}

	nlohmann::json MaterialDescJsonConverter::ToJson(const MaterialDescSource& source)
	{
		const MaterialDescSource normalized = MaterialDescLoader::NormalizeSource(source);

		nlohmann::json json = nlohmann::json::object();
		json[Keys::MaterialId] = normalized.materialId;
		json[Keys::MaterialName] = normalized.materialName;
		json[Keys::SourcePath] = normalized.sourcePath;
		json[Keys::SourceKind] = ToString(normalized.sourceKind);
		json[Keys::PreferPbrWorkflow] = normalized.preferPbrWorkflow;

		json[Keys::Legacy] = {
			{ Keys::LegacyColor, ToJsonVector4(normalized.legacyColor) },
			{ Keys::LegacyShininess, normalized.legacyShininess },
			{ Keys::LegacyReflectionRate, normalized.legacyReflectionRate },
			{ Keys::LegacyRoughness, normalized.legacyRoughness },
			{ Keys::LegacyBaseColorTexture, normalized.baseColorTexturePath },
			{ Keys::LegacyUsePointSampling, normalized.usePointSampling },
		};

		json[Keys::Pbr] = {
			{ Keys::PbrBaseColor, ToJsonVector4(normalized.baseColorFactor) },
			{ Keys::PbrMetallic, normalized.metallicFactor },
			{ Keys::PbrRoughness, normalized.roughnessFactor },
			{ Keys::PbrNormalScale, normalized.normalScale },
			{ Keys::PbrOcclusionStrength, normalized.occlusionStrength },
			{ Keys::PbrEmissiveColor, ToJsonVector4(normalized.emissiveFactor) },
			{ Keys::PbrEmissiveStrength, 1.0f },
			{ Keys::PbrBaseColorTexture, normalized.baseColorTexturePath },
			{ Keys::PbrNormalTexture, normalized.normalTexturePath },
			{ Keys::PbrMetallicRoughnessTexture, normalized.metallicRoughnessTexturePath },
			{ Keys::PbrOcclusionTexture, normalized.occlusionTexturePath },
			{ Keys::PbrEmissiveTexture, normalized.emissiveTexturePath },
		};

		json[Keys::TextureSlots] = nlohmann::json::array();
		for (const auto& slot : normalized.textureSlots)
		{
			if (slot.semantic.empty() && slot.texturePath.empty())
			{
				continue;
			}
			json[Keys::TextureSlots].push_back({
				{ Keys::TextureSlotSemantic, slot.semantic },
				{ Keys::TextureSlotPath, slot.texturePath },
			});
		}

		// ここではJsonオブジェクトを返すだけで、ファイル保存やMaterialRepository登録は呼び出し側の明示的な責務にする。
		return json;
	}

	const char* MaterialDescJsonConverter::ToString(MaterialSourceKind kind)
	{
		switch (kind)
		{
		case MaterialSourceKind::Json:
			return "json";
		case MaterialSourceKind::Gltf:
			return "gltf";
		case MaterialSourceKind::MaterialEditor:
			return "materialEditor";
		case MaterialSourceKind::Manual:
		default:
			return "manual";
		}
	}

	MaterialSourceKind MaterialDescJsonConverter::SourceKindFromString(const std::string& text)
	{
		if (text == "json")
		{
			return MaterialSourceKind::Json;
		}
		if (text == "gltf")
		{
			return MaterialSourceKind::Gltf;
		}
		if (text == "materialEditor")
		{
			return MaterialSourceKind::MaterialEditor;
		}
		return MaterialSourceKind::Manual;
	}
}
