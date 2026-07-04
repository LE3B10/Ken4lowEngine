#define NOMINMAX
#include "WorldGaugeComponent.h"

#include "CameraManager.h"
#include "GameViewportConstants.h"
#include "Matrix4x4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		Vector2 ReadVector2FromJson(const nlohmann::json& json, const char* key, const Vector2& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 2)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>()
			};
		}
	}

	WorldGaugeComponent::~WorldGaugeComponent() = default;

	void WorldGaugeComponent::Initialize()
	{
		SceneComponent::Initialize();
		gauge_.Initialize();
	}

	void WorldGaugeComponent::Draw()
	{
		// WorldGaugeComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void WorldGaugeComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示または無効な状態の場合は描画しない
		}

		Vector2 screenPosition{};
		if (!UpdateScreenPosition(screenPosition))
		{
			return; // カメラ背面など描画できない位置の場合は描画しない
		}

		gauge_.DrawGaugeAt(screenPosition);
	}

	bool WorldGaugeComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_;
	}

	void WorldGaugeComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		SceneComponent::DrawImGui();

		ImGui::SeparatorText("ワールドゲージコンポーネント");

		float value = GetValue();
		float maxValue = GetMaxValue();
		if (ImGui::DragFloat("値", &value, 1.0f, 0.0f, maxValue))
		{
			SetValue(value);
		}
		if (ImGui::DragFloat("最大値", &maxValue, 1.0f, 0.0001f, 1000000.0f))
		{
			SetMaxValue(maxValue);
		}

		ImGui::DragFloat2("スクリーンオフセット", &screenOffset_.x, 1.0f);

		Vector2 size = GetSize();
		if (ImGui::DragFloat2("サイズ", &size.x, 1.0f, 0.0f, 4096.0f))
		{
			SetSize(size);
		}

		Vector4 backgroundColor = GetBackgroundColor();
		if (ImGui::ColorEdit4("背景色", &backgroundColor.x))
		{
			SetBackgroundColor(backgroundColor);
		}

		Vector4 fillColor = GetFillColor();
		if (ImGui::ColorEdit4("塗り色", &fillColor.x))
		{
			SetFillColor(fillColor);
		}

		Vector4 borderColor = GetBorderColor();
		if (ImGui::ColorEdit4("枠色", &borderColor.x))
		{
			SetBorderColor(borderColor);
		}

		float borderThickness = GetBorderThickness();
		if (ImGui::DragFloat("枠の太さ", &borderThickness, 0.1f, 0.0f, 64.0f))
		{
			SetBorderThickness(borderThickness);
		}

		ImGui::Checkbox("表示", &visible_);
		ImGui::Checkbox("カメラ背面で非表示", &hideWhenBehindCamera_);

		const char* fillDirectionItems[] = { "左から右", "右から左", "上から下", "下から上" };
		int fillDirectionIndex = static_cast<int>(GetFillDirection());
		if (ImGui::Combo("塗り方向", &fillDirectionIndex, fillDirectionItems, static_cast<int>(std::size(fillDirectionItems))))
		{
			SetFillDirection(static_cast<GaugeComponent::FillDirection>(std::clamp(fillDirectionIndex, 0, static_cast<int>(std::size(fillDirectionItems)) - 1)));
		}
#endif // USE_IMGUI
	}

	void WorldGaugeComponent::Finalize()
	{
		gauge_.Finalize();
	}

	void WorldGaugeComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		nlohmann::json gaugeJson;
		gauge_.ToJson(gaugeJson);

		outJson["Class"] = GetClassTypeName(); // WorldGaugeComponentとして保存する
		outJson["Value"] = gaugeJson.value("Value", GetValue());
		outJson["MaxValue"] = gaugeJson.value("MaxValue", GetMaxValue());
		outJson["ScreenOffset"] = { screenOffset_.x, screenOffset_.y };
		outJson["Size"] = gaugeJson.value("Size", nlohmann::json::array({ GetSize().x, GetSize().y }));
		outJson["BackgroundColor"] = gaugeJson.value("BackgroundColor", nlohmann::json::array({ GetBackgroundColor().x, GetBackgroundColor().y, GetBackgroundColor().z, GetBackgroundColor().w }));
		outJson["FillColor"] = gaugeJson.value("FillColor", nlohmann::json::array({ GetFillColor().x, GetFillColor().y, GetFillColor().z, GetFillColor().w }));
		outJson["BorderColor"] = gaugeJson.value("BorderColor", nlohmann::json::array({ GetBorderColor().x, GetBorderColor().y, GetBorderColor().z, GetBorderColor().w }));
		outJson["BorderThickness"] = gaugeJson.value("BorderThickness", GetBorderThickness());
		outJson["FillDirection"] = gaugeJson.value("FillDirection", std::string("LeftToRight"));
		outJson["Visible"] = visible_;
		outJson["HideWhenBehindCamera"] = hideWhenBehindCamera_;
	}

	void WorldGaugeComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		nlohmann::json gaugeJson = inJson;
		gaugeJson["Class"] = "GaugeComponent";
		gaugeJson["Position"] = { 0.0f, 0.0f };
		gaugeJson["Visible"] = true;
		gauge_.FromJson(gaugeJson);

		screenOffset_ = ReadVector2FromJson(inJson, "ScreenOffset", screenOffset_);

		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>();
		}

		if (inJson.contains("HideWhenBehindCamera") && inJson["HideWhenBehindCamera"].is_boolean())
		{
			hideWhenBehindCamera_ = inJson["HideWhenBehindCamera"].get<bool>();
		}
	}

	bool WorldGaugeComponent::UpdateScreenPosition(Vector2& outScreenPosition) const
	{
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Vector3 worldPosition = GetWorldPosition();

		const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];

		if (hideWhenBehindCamera_ && clipW <= 0.0f)
		{
			return false; // カメラ背面のGaugeは描画しない
		}

		if (std::fabs(clipW) <= 0.0001f)
		{
			return false; // NDC変換できない位置は描画しない
		}

		const float ndcX = clipX / clipW;
		const float ndcY = clipY / clipW;

		const float screenWidth = static_cast<float>(GameViewportConstants::Width);
		const float screenHeight = static_cast<float>(GameViewportConstants::Height);

		const Vector2 size = GetSize();
		outScreenPosition.x = (ndcX + 1.0f) * 0.5f * screenWidth + screenOffset_.x - size.x * 0.5f;
		outScreenPosition.y = (1.0f - ndcY) * 0.5f * screenHeight + screenOffset_.y - size.y * 0.5f;

		return true;
	}
}
