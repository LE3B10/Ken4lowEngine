#include "LightComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		Vector3 ReadVector3FromJson(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 3)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>()
			};
		}
	}

	void LightComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // ライトの基準位置を編集できるようにする

#ifdef USE_IMGUI
		ImGui::SeparatorText("ライトコンポーネント");
		ImGui::Checkbox("有効", &enabled_);
		ImGui::ColorEdit3("色", &color_.x);
		ImGui::DragFloat("強さ", &intensity_, 0.05f, 0.0f, 100.0f);
		ImGui::DragFloat("範囲", &range_, 0.1f, 0.0f, 1000.0f);
#endif // USE_IMGUI
	}

	void LightComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // LightComponentとして保存する
		outJson["Color"] = { color_.x, color_.y, color_.z };
		outJson["Intensity"] = intensity_;
		outJson["Range"] = range_;
		outJson["Enabled"] = enabled_;
	}

	void LightComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		color_ = ReadVector3FromJson(inJson, "Color", color_);

		if (inJson.contains("Intensity") && inJson["Intensity"].is_number())
		{
			intensity_ = inJson["Intensity"].get<float>(); // ライトの強さを復元する
		}

		if (inJson.contains("Range") && inJson["Range"].is_number())
		{
			range_ = inJson["Range"].get<float>(); // ライトの届く範囲を復元する
		}

		if (inJson.contains("Enabled") && inJson["Enabled"].is_boolean())
		{
			enabled_ = inJson["Enabled"].get<bool>(); // ライトの有効状態を復元する
		}
	}
}
