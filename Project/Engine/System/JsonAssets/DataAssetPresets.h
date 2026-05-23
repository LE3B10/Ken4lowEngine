#pragma once

#include "JsonSerializable.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>

namespace Ken4lowEngine
{
	class Sprite;

	struct LightPreset : public JsonSerializable
	{
		Vector3 directionalDirection = { 0.3f, -1.0f, 0.2f };
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		bool enableShadow = true;
		float shadowBias = 0.0f;
		float normalBias = 0.025f;
		float shadowStrength = 0.6f;
		uint32_t shadowMapSize = 2048;
		float shadowWidth = 35.0f;
		float shadowHeight = 35.0f;
		float shadowNearZ = 0.1f;
		float shadowFarZ = 120.0f;
		uint32_t shadowFocusMode = 0;

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct PostEffectPreset : public JsonSerializable
	{
		bool enabled = true;
		std::string activeEffect = "None";
		float bloomIntensity = 0.0f;
		float vignetteIntensity = 0.0f;
		float grayscale = 0.0f;
		float sepia = 0.0f;
		float fade = 0.0f;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct Object3DPreset : public JsonSerializable
	{
		std::string modelPath;
		std::string texturePath;
		Vector3 position{};
		Vector3 rotation{};
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		bool visible = true;
		bool castShadow = true;
		bool receiveShadow = true;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct SpritePreset : public JsonSerializable
	{
		std::string texturePath;
		Vector2 position{};
		Vector2 size = { 128.0f, 128.0f };
		float rotation = 0.0f;
		Vector2 anchor = { 0.5f, 0.5f };
		Vector4 color = { 1,1,1,1 };
		bool visible = true;
		int layer = 0;
		Vector2 pivot = { 0.5f, 0.5f };
		Vector2 uvPosition = { 0.0f, 0.0f };
		Vector2 uvSize = { 1.0f, 1.0f };
		bool enableAlpha = true;
		int drawOrder = 0;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	void ApplySpritePreset(Sprite& sprite, const SpritePreset& preset);

	struct ParticlePreset : public JsonSerializable
	{
		std::string emitterType = "Default";
		Vector3 position{};
		float spawnRate = 10.0f;
		float lifetime = 1.0f;
		float speed = 1.0f;
		Vector4 color = { 1,1,1,1 };
		float size = 1.0f;
		bool loop = true;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};
}
