#include "LightComponent.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

		std::string LightTypeToString(LightComponent::LightType lightType)
		{
			switch (lightType)
			{
			case LightComponent::LightType::None: return "None";
			case LightComponent::LightType::Directional: return "Directional";
			case LightComponent::LightType::Point: return "Point";
			case LightComponent::LightType::Spot: return "Spot";
			case LightComponent::LightType::RectArea: return "RectArea";
			case LightComponent::LightType::SphereArea: return "SphereArea";
			default: return "Point";
			}
		}

		LightComponent::LightType LightTypeFromString(const std::string& lightType)
		{
			if (lightType == "None") { return LightComponent::LightType::None; }
			if (lightType == "Directional") { return LightComponent::LightType::Directional; }
			if (lightType == "Point") { return LightComponent::LightType::Point; }
			if (lightType == "Spot") { return LightComponent::LightType::Spot; }
			if (lightType == "RectArea") { return LightComponent::LightType::RectArea; }
			if (lightType == "SphereArea") { return LightComponent::LightType::SphereArea; }
			return LightComponent::LightType::Point;
		}

		bool UsesRange(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::Point ||
				lightType == LightComponent::LightType::Spot ||
				lightType == LightComponent::LightType::RectArea ||
				lightType == LightComponent::LightType::SphereArea;
		}

		bool UsesDirection(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::Directional ||
				lightType == LightComponent::LightType::Spot ||
				lightType == LightComponent::LightType::RectArea;
		}

		bool UsesAreaSize(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::RectArea ||
				lightType == LightComponent::LightType::SphereArea;
		}

		std::vector<ComponentPropertyChoice> LightTypeChoices()
		{
			return {
				{ "None", "なし / None" },
				{ "Directional", "平行光源 / Directional" },
				{ "Point", "点光源 / Point" },
				{ "Spot", "スポットライト / Spot" },
				{ "RectArea", "矩形エリアライト / RectArea" },
				{ "SphereArea", "球エリアライト / SphereArea" },
			};
		}
	}

	void LightComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // ライトの基準位置を編集できるようにする

#ifdef USE_IMGUI
		ImGui::SeparatorText("ライトコンポーネント");
		ImGui::TextUnformatted("Edit per-actor light values here. Local transform is edited in Scene Component.");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
		if (UsesDirection(lightType_))
		{
			const Vector3 direction = CalculateDirection();
			ImGui::Text("ライト方向: %.2f, %.2f, %.2f", direction.x, direction.y, direction.z);
		}
#endif // USE_IMGUI
	}

	void LightComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // LightComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<LightComponent*>(this)->CreateProperties(), outJson);
	}

	void LightComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("LightType") && inJson["LightType"].is_string())
		{
			lightType_ = LightTypeFromString(inJson["LightType"].get<std::string>()); // 不正な種類はPointへフォールバックする
		}

		ComponentPropertyUtility::FromJson(CreateProperties(true), inJson);
		Sanitize();
	}

	std::vector<ComponentProperty> LightComponent::CreateProperties(bool includeAll)
	{
		std::vector<ComponentProperty> properties = {
			ComponentProperty{
				"LightType",
				"ライト種類",
				ComponentPropertyType::String,
				[this]() -> ComponentPropertyValue { return LightTypeToString(lightType_); },
				[this](const ComponentPropertyValue& value)
				{
					if (const std::string* typedValue = std::get_if<std::string>(&value))
					{
						lightType_ = LightTypeFromString(*typedValue); // LightManagerの既存lightTypeに対応する種類へ変換する
					}
				},
				0.0f,
				0.0f,
				0.1f,
				false,
				LightTypeChoices()
			},
			ComponentProperty{
				"Enabled",
				"有効",
				ComponentPropertyType::Bool,
				[this]() -> ComponentPropertyValue { return enabled_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const bool* typedValue = std::get_if<bool>(&value))
					{
						enabled_ = *typedValue; // ライトの有効状態を更新する
					}
				}
			},
			ComponentProperty{
				"Color",
				"色",
				ComponentPropertyType::Vector3,
				[this]() -> ComponentPropertyValue { return color_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const Vector3* typedValue = std::get_if<Vector3>(&value))
					{
						color_ = *typedValue; // ライト色を更新する
					}
				},
				0.0f,
				1.0f,
				0.01f,
				true
			},
			ComponentProperty{
				"Intensity",
				"強さ",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return intensity_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						intensity_ = *typedValue; // ライトの強さを更新する
					}
				},
				0.0f,
				100.0f,
				0.05f,
				true
			},
		};

		if (includeAll || UsesRange(lightType_))
		{
			properties.push_back(ComponentProperty{
				"Range",
				"範囲",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return range_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						range_ = *typedValue; // ライトの届く範囲を更新する
						Sanitize();
					}
				},
				0.0f,
				1000.0f,
				0.1f,
				true
			});

			properties.push_back(ComponentProperty{
				"Decay",
				"減衰",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return decay_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						decay_ = *typedValue; // 距離減衰の強さを更新する
						Sanitize();
					}
				},
				0.0f,
				10.0f,
				0.05f,
				true
			});
		}

		if (includeAll || lightType_ == LightType::Spot)
		{
			properties.push_back(ComponentProperty{
				"InnerAngle",
				"内角度",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return innerAngle_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						innerAngle_ = *typedValue; // SpotLightの内側角度を更新する
						Sanitize();
					}
				},
				0.0f,
				179.0f,
				0.1f,
				true
			});

			properties.push_back(ComponentProperty{
				"OuterAngle",
				"外角度",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return outerAngle_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						outerAngle_ = *typedValue; // SpotLightの外側角度を更新する
						Sanitize();
					}
				},
				0.1f,
				179.0f,
				0.1f,
				true
			});
		}

		if (includeAll || UsesAreaSize(lightType_))
		{
			properties.push_back(ComponentProperty{
				"AreaSize",
				"エリアサイズ",
				ComponentPropertyType::Vector3,
				[this]() -> ComponentPropertyValue { return areaSize_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const Vector3* typedValue = std::get_if<Vector3>(&value))
					{
						areaSize_ = *typedValue; // 既存AreaLight近似のサイズを更新する
						Sanitize();
					}
				},
				0.0f,
				50.0f,
				0.05f,
				true
			});
		}

		return properties;
	}

	void LightComponent::Sanitize()
	{
		range_ = std::max(range_, 0.0f);
		decay_ = std::clamp(decay_, 0.0f, 10.0f);
		outerAngle_ = std::clamp(outerAngle_, 0.1f, 179.0f);
		innerAngle_ = std::clamp(innerAngle_, 0.0f, outerAngle_);
		areaSize_.x = std::max(areaSize_.x, 0.0f);
		areaSize_.y = std::max(areaSize_.y, 0.0f);
		areaSize_.z = std::max(areaSize_.z, 0.0f);
	}

	Vector3 LightComponent::CalculateDirection() const
	{
		const Vector3& rotation = GetWorldRotation();
		const float pitch = rotation.x;
		const float yaw = rotation.y;
		const float cp = std::cos(pitch);
		const float sp = std::sin(pitch);
		const float cy = std::cos(yaw);
		const float sy = std::sin(yaw);

		return Vector3::NormalizeSafe({ sy * cp, -sp, cy * cp }, { 0.0f, -1.0f, 0.0f });
	}
}
