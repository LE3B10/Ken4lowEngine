#include "GpuParticleEffectSerializer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <json.hpp>

namespace Ken4lowEngine
{
	using json = nlohmann::json;
	namespace fs = std::filesystem;

	namespace
	{
		json ToJson(const Vector2& value) { return { value.x, value.y }; }
		json ToJson(const Vector3& value) { return { value.x, value.y, value.z }; }
		json ToJson(const Vector4& value) { return { value.x, value.y, value.z, value.w }; }

		template<class T>
		void ReadOptional(const json& source, const char* key, T& value)
		{
			if (!source.contains(key)) return;
			try { value = source.at(key).get<T>(); }
			catch (const json::exception&) { /* 古いJSONや型不正でも既定値を維持してクラッシュさせない。 */ }
		}

		void ReadVector2(const json& source, const char* key, Vector2& value)
		{
			if (!source.contains(key)) return;
			try
			{
				const auto& array = source.at(key);
				if (array.is_array() && array.size() >= 2 && array[0].is_number() && array[1].is_number())
					value = { array[0].get<float>(), array[1].get<float>() };
			}
			catch (const json::exception&) {}
		}

		void ReadVector3(const json& source, const char* key, Vector3& value)
		{
			if (!source.contains(key)) return;
			try
			{
				const auto& array = source.at(key);
				if (array.is_array() && array.size() >= 3 && array[0].is_number() && array[1].is_number() && array[2].is_number())
					value = { array[0].get<float>(), array[1].get<float>(), array[2].get<float>() };
			}
			catch (const json::exception&) {}
		}

		void ReadVector4(const json& source, const char* key, Vector4& value)
		{
			if (!source.contains(key)) return;
			try
			{
				const auto& array = source.at(key);
				if (array.is_array() && array.size() >= 4 && array[0].is_number() && array[1].is_number() && array[2].is_number() && array[3].is_number())
					value = { array[0].get<float>(), array[1].get<float>(), array[2].get<float>(), array[3].get<float>() };
			}
			catch (const json::exception&) {}
		}

		std::string ReadStringOr(const json& source, const char* key, const std::string& fallback)
		{
			if (!source.contains(key) || !source.at(key).is_string()) return fallback;
			return source.at(key).get<std::string>();
		}
	}

	std::string ToString(GpuParticleRenderType type)
	{
		return type == GpuParticleRenderType::Mesh ? "Mesh" : "Sprite";
	}

	GpuParticleRenderType GpuParticleRenderTypeFromString(const std::string& text)
	{
		return text == "Mesh" ? GpuParticleRenderType::Mesh : GpuParticleRenderType::Sprite;
	}

	std::string ToString(GpuParticleBlendMode mode)
	{
		switch (mode)
		{
		case GpuParticleBlendMode::Additive: return "Additive";
		case GpuParticleBlendMode::Multiply: return "Multiply";
		case GpuParticleBlendMode::Alpha:
		default: return "Alpha";
		}
	}

	GpuParticleBlendMode GpuParticleBlendModeFromString(const std::string& text)
	{
		if (text == "Additive") return GpuParticleBlendMode::Additive;
		if (text == "Multiply") return GpuParticleBlendMode::Multiply;
		return GpuParticleBlendMode::Alpha;
	}

	std::string ToString(GpuParticleSpawnShape shape)
	{
		switch (shape)
		{
		case GpuParticleSpawnShape::Sphere: return "Sphere";
		case GpuParticleSpawnShape::Box: return "Box";
		case GpuParticleSpawnShape::Point:
		default: return "Point";
		}
	}

	GpuParticleSpawnShape GpuParticleSpawnShapeFromString(const std::string& text)
	{
		if (text == "Sphere") return GpuParticleSpawnShape::Sphere;
		if (text == "Box") return GpuParticleSpawnShape::Box;
		return GpuParticleSpawnShape::Point;
	}

	const char* GpuParticleEffectSerializer::ToString(GpuParticleRenderType value)
	{
		return value == GpuParticleRenderType::Mesh ? "Mesh" : "Sprite";
	}

	bool GpuParticleEffectSerializer::TryParseRenderType(const std::string& text, GpuParticleRenderType& outValue)
	{
		if (text != "Sprite" && text != "Mesh") return false;
		outValue = GpuParticleRenderTypeFromString(text);
		return true;
	}

	bool GpuParticleEffectSerializer::Load(GpuParticleEffectDesc& desc, const std::string& filePath)
	{
		try
		{
			std::ifstream input(filePath);
			if (!input.is_open()) return false;
			json root;
			input >> root;
			if (!root.is_object()) return false;

			GpuParticleEffectDesc effect = CreateDefaultGpuParticleEffectDesc();
			ReadOptional(root, "effectName", effect.effectName);
			if (root.contains("emitters"))
			{
				if (!root.at("emitters").is_array()) return false;
				effect.emitters.clear();
				for (const auto& source : root.at("emitters"))
				{
					if (!source.is_object()) continue;
					const auto renderType = GpuParticleRenderTypeFromString(ReadStringOr(source, "renderType", "Sprite"));
					GpuParticleEmitterDesc emitter = renderType == GpuParticleRenderType::Mesh
						? CreateDefaultMeshEmitterDesc() : CreateDefaultSpriteEmitterDesc();
					emitter.renderType = renderType;

					ReadOptional(source, "name", emitter.name);
					ReadOptional(source, "texturePath", emitter.texturePath);
					ReadOptional(source, "meshPath", emitter.meshPath);
					ReadOptional(source, "maxParticles", emitter.maxParticles);
					ReadOptional(source, "loop", emitter.loop);
					ReadOptional(source, "duration", emitter.duration);
					ReadOptional(source, "spawnRate", emitter.spawnRate);
					ReadOptional(source, "burstCount", emitter.burstCount);
					ReadOptional(source, "lifeTime", emitter.lifeTime);
					ReadOptional(source, "lifeTimeRandom", emitter.lifeTimeRandom);
					ReadVector3(source, "position", emitter.position);
					ReadVector3(source, "positionRandom", emitter.positionRandom);
					emitter.spawnShape = GpuParticleSpawnShapeFromString(ReadStringOr(source, "spawnShape", "Point"));
					ReadOptional(source, "spawnRadius", emitter.spawnRadius);
					ReadVector3(source, "spawnBoxSize", emitter.spawnBoxSize);
					ReadVector3(source, "velocity", emitter.velocity);
					ReadVector3(source, "velocityRandom", emitter.velocityRandom);
					ReadVector3(source, "gravity", emitter.gravity);
					ReadOptional(source, "damping", emitter.damping);
					ReadOptional(source, "speed", emitter.speed);
					ReadOptional(source, "speedRandom", emitter.speedRandom);
					ReadVector2(source, "startSize", emitter.startSize);
					ReadVector2(source, "endSize", emitter.endSize);
					ReadOptional(source, "sizeRandom", emitter.sizeRandom);
					ReadVector4(source, "startColor", emitter.startColor);
					ReadVector4(source, "endColor", emitter.endColor);
					ReadVector4(source, "colorRandom", emitter.colorRandom);
					ReadOptional(source, "alphaFade", emitter.alphaFade);
					ReadOptional(source, "startRotation", emitter.startRotation);
					ReadOptional(source, "rotationSpeed", emitter.rotationSpeed);
					ReadOptional(source, "rotationRandom", emitter.rotationRandom);
					ReadOptional(source, "billboard", emitter.billboard);
					emitter.blendMode = GpuParticleBlendModeFromString(ReadStringOr(source, "blendMode", "Alpha"));
					ReadOptional(source, "useSpriteSheet", emitter.useSpriteSheet);
					ReadOptional(source, "spriteSheetRows", emitter.spriteSheetRows);
					ReadOptional(source, "spriteSheetColumns", emitter.spriteSheetColumns);
					ReadOptional(source, "spriteSheetFrameRate", emitter.spriteSheetFrameRate);
					ReadVector3(source, "startScale3D", emitter.startScale3D);
					ReadVector3(source, "endScale3D", emitter.endScale3D);
					ReadVector3(source, "angularVelocity", emitter.angularVelocity);
					ReadVector3(source, "angularVelocityRandom", emitter.angularVelocityRandom);
					emitter.spriteSheetRows = (std::max)(emitter.spriteSheetRows, 1);
					emitter.spriteSheetColumns = (std::max)(emitter.spriteSheetColumns, 1);
					effect.emitters.push_back(std::move(emitter));
				}
			}

			desc = std::move(effect);
			return true;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	bool GpuParticleEffectSerializer::Save(const GpuParticleEffectDesc& effect, const std::string& filePath)
	{
		try
		{
			json root;
			root["effectName"] = effect.effectName;
			root["emitters"] = json::array();
			for (const auto& e : effect.emitters)
			{
				root["emitters"].push_back({
					{ "name", e.name }, { "renderType", Ken4lowEngine::ToString(e.renderType) },
					{ "texturePath", e.texturePath }, { "meshPath", e.meshPath },
					{ "maxParticles", e.maxParticles }, { "loop", e.loop }, { "duration", e.duration },
					{ "spawnRate", e.spawnRate }, { "burstCount", e.burstCount },
					{ "lifeTime", e.lifeTime }, { "lifeTimeRandom", e.lifeTimeRandom },
					{ "position", ToJson(e.position) }, { "positionRandom", ToJson(e.positionRandom) },
					{ "spawnShape", Ken4lowEngine::ToString(e.spawnShape) }, { "spawnRadius", e.spawnRadius },
					{ "spawnBoxSize", ToJson(e.spawnBoxSize) },
					{ "velocity", ToJson(e.velocity) }, { "velocityRandom", ToJson(e.velocityRandom) },
					{ "gravity", ToJson(e.gravity) }, { "damping", e.damping },
					{ "speed", e.speed }, { "speedRandom", e.speedRandom },
					{ "startSize", ToJson(e.startSize) }, { "endSize", ToJson(e.endSize) }, { "sizeRandom", e.sizeRandom },
					{ "startColor", ToJson(e.startColor) }, { "endColor", ToJson(e.endColor) },
					{ "colorRandom", ToJson(e.colorRandom) }, { "alphaFade", e.alphaFade },
					{ "startRotation", e.startRotation }, { "rotationSpeed", e.rotationSpeed }, { "rotationRandom", e.rotationRandom },
					{ "billboard", e.billboard }, { "blendMode", Ken4lowEngine::ToString(e.blendMode) },
					{ "useSpriteSheet", e.useSpriteSheet }, { "spriteSheetRows", e.spriteSheetRows },
					{ "spriteSheetColumns", e.spriteSheetColumns }, { "spriteSheetFrameRate", e.spriteSheetFrameRate },
					{ "startScale3D", ToJson(e.startScale3D) }, { "endScale3D", ToJson(e.endScale3D) },
					{ "angularVelocity", ToJson(e.angularVelocity) }, { "angularVelocityRandom", ToJson(e.angularVelocityRandom) }
				});
			}

			const fs::path path(filePath);
			if (path.has_parent_path()) fs::create_directories(path.parent_path());
			std::ofstream output(filePath);
			if (!output.is_open()) return false;
			output << root.dump(4);
			return output.good();
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	std::optional<GpuParticleEffectDesc> GpuParticleEffectSerializer::LoadFromFile(const std::string& filePath)
	{
		GpuParticleEffectDesc desc{};
		if (!Load(desc, filePath)) return std::nullopt;
		return desc;
	}

	bool GpuParticleEffectSerializer::SaveToFile(const GpuParticleEffectDesc& effect, const std::string& filePath)
	{
		return Save(effect, filePath);
	}
} // namespace Ken4lowEngine
