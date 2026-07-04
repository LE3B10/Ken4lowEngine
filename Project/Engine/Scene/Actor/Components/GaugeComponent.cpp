#define NOMINMAX
#include "GaugeComponent.h"

#include "SpriteManager.h"

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
		constexpr const char* kGaugeTexturePath = "Effects/white.dds";
		constexpr float kMinMaxValue = 0.0001f;

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

		Vector4 ReadVector4FromJson(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>(),
				json[key][3].get<float>()
			};
		}
	}

	GaugeComponent::~GaugeComponent() = default;

	void GaugeComponent::Initialize()
	{
		EnsureSprites();
		NormalizeValues();
	}

	void GaugeComponent::Draw()
	{
		// GaugeComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void GaugeComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示または無効な状態の場合は描画しない
		}

		DrawGaugeAt(position_);
	}

	bool GaugeComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_;
	}

	void GaugeComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ゲージコンポーネント");

		ImGui::DragFloat("値", &value_, 1.0f, 0.0f, maxValue_);
		ImGui::DragFloat("最大値", &maxValue_, 1.0f, kMinMaxValue, 1000000.0f);
		NormalizeValues();

		ImGui::DragFloat2("位置", &position_.x, 1.0f);
		ImGui::DragFloat2("サイズ", &size_.x, 1.0f, 0.0f, 4096.0f);
		ImGui::ColorEdit4("背景色", &backgroundColor_.x);
		ImGui::ColorEdit4("塗り色", &fillColor_.x);
		ImGui::ColorEdit4("枠色", &borderColor_.x);
		ImGui::DragFloat("枠の太さ", &borderThickness_, 0.1f, 0.0f, 64.0f);
		ImGui::Checkbox("表示", &visible_);

		const char* fillDirectionItems[] = { "左から右", "右から左", "上から下", "下から上" };
		int fillDirectionIndex = static_cast<int>(fillDirection_);
		if (ImGui::Combo("塗り方向", &fillDirectionIndex, fillDirectionItems, static_cast<int>(std::size(fillDirectionItems))))
		{
			fillDirection_ = static_cast<FillDirection>(std::clamp(fillDirectionIndex, 0, static_cast<int>(std::size(fillDirectionItems)) - 1));
		}
#endif // USE_IMGUI
	}

	void GaugeComponent::Finalize()
	{
		if (backgroundSprite_)
		{
			backgroundSprite_->Finalize();
			backgroundSprite_.reset();
		}

		if (fillSprite_)
		{
			fillSprite_->Finalize();
			fillSprite_.reset();
		}

		for (auto& borderSprite : borderSprites_)
		{
			if (borderSprite)
			{
				borderSprite->Finalize();
				borderSprite.reset();
			}
		}
	}

	void GaugeComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // GaugeComponentとして保存する
		outJson["Value"] = value_;
		outJson["MaxValue"] = maxValue_;
		outJson["Position"] = { position_.x, position_.y };
		outJson["Size"] = { size_.x, size_.y };
		outJson["BackgroundColor"] = { backgroundColor_.x, backgroundColor_.y, backgroundColor_.z, backgroundColor_.w };
		outJson["FillColor"] = { fillColor_.x, fillColor_.y, fillColor_.z, fillColor_.w };
		outJson["BorderColor"] = { borderColor_.x, borderColor_.y, borderColor_.z, borderColor_.w };
		outJson["BorderThickness"] = borderThickness_;
		outJson["FillDirection"] = FillDirectionToString(fillDirection_);
		outJson["Visible"] = visible_;
	}

	void GaugeComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

		if (inJson.contains("Value") && inJson["Value"].is_number())
		{
			value_ = inJson["Value"].get<float>();
		}

		if (inJson.contains("MaxValue") && inJson["MaxValue"].is_number())
		{
			maxValue_ = inJson["MaxValue"].get<float>();
		}

		position_ = ReadVector2FromJson(inJson, "Position", position_);
		size_ = ReadVector2FromJson(inJson, "Size", size_);
		backgroundColor_ = ReadVector4FromJson(inJson, "BackgroundColor", backgroundColor_);
		fillColor_ = ReadVector4FromJson(inJson, "FillColor", fillColor_);
		borderColor_ = ReadVector4FromJson(inJson, "BorderColor", borderColor_);

		if (inJson.contains("BorderThickness") && inJson["BorderThickness"].is_number())
		{
			borderThickness_ = inJson["BorderThickness"].get<float>();
		}

		if (inJson.contains("FillDirection") && inJson["FillDirection"].is_string())
		{
			fillDirection_ = FillDirectionFromString(inJson["FillDirection"].get<std::string>(), fillDirection_);
		}

		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>();
		}

		NormalizeValues();
	}

	void GaugeComponent::SetValue(float value)
	{
		value_ = value;
		NormalizeValues();
	}

	void GaugeComponent::SetMaxValue(float maxValue)
	{
		maxValue_ = maxValue;
		NormalizeValues();
	}

	void GaugeComponent::DrawGaugeAt(const Vector2& screenPosition)
	{
		NormalizeValues();
		EnsureSprites();

		if (!backgroundSprite_ || !fillSprite_)
		{
			return; // ゲージ描画用Spriteが生成できない場合は描画しない
		}

		SpriteManager::GetInstance()->SetRenderSetting_UI();
		DrawColoredRect(*backgroundSprite_, screenPosition, size_, backgroundColor_);

		Vector2 fillPosition{};
		Vector2 fillSize{};
		CalculateFillRect(fillPosition, fillSize);
		if (fillSize.x > 0.0f && fillSize.y > 0.0f)
		{
			DrawColoredRect(*fillSprite_, { screenPosition.x + fillPosition.x, screenPosition.y + fillPosition.y }, fillSize, fillColor_);
		}

		DrawBorder(screenPosition, size_);
	}

	void GaugeComponent::NormalizeValues()
	{
		maxValue_ = std::max(maxValue_, kMinMaxValue);
		value_ = std::clamp(value_, 0.0f, maxValue_);
		size_.x = std::max(size_.x, 0.0f);
		size_.y = std::max(size_.y, 0.0f);
		borderThickness_ = std::max(borderThickness_, 0.0f);
	}

	void GaugeComponent::EnsureSprites()
	{
		if (!backgroundSprite_)
		{
			backgroundSprite_ = std::make_unique<Sprite>();
			backgroundSprite_->Initialize(kGaugeTexturePath);
		}

		if (!fillSprite_)
		{
			fillSprite_ = std::make_unique<Sprite>();
			fillSprite_->Initialize(kGaugeTexturePath);
		}

		for (auto& borderSprite : borderSprites_)
		{
			if (!borderSprite)
			{
				borderSprite = std::make_unique<Sprite>();
				borderSprite->Initialize(kGaugeTexturePath);
			}
		}
	}

	void GaugeComponent::DrawColoredRect(Sprite& sprite, const Vector2& position, const Vector2& size, const Vector4& color, const Vector2& anchor)
	{
		sprite.SetPosition(position);
		sprite.SetSize(size);
		sprite.SetColor(color);
		sprite.SetAnchorPoint(anchor);
		sprite.SetRotation(0.0f);
		sprite.Update();
		sprite.Draw();
	}

	void GaugeComponent::DrawBorder(const Vector2& position, const Vector2& size)
	{
		if (borderThickness_ <= 0.0f || borderColor_.w <= 0.0f)
		{
			return; // 枠線が不要な場合は描画しない
		}

		const float thickness = std::min(borderThickness_, std::min(size.x, size.y) * 0.5f);
		if (thickness <= 0.0f)
		{
			return;
		}

		DrawColoredRect(*borderSprites_[0], position, { size.x, thickness }, borderColor_);
		DrawColoredRect(*borderSprites_[1], { position.x, position.y + size.y - thickness }, { size.x, thickness }, borderColor_);
		DrawColoredRect(*borderSprites_[2], position, { thickness, size.y }, borderColor_);
		DrawColoredRect(*borderSprites_[3], { position.x + size.x - thickness, position.y }, { thickness, size.y }, borderColor_);
	}

	void GaugeComponent::CalculateFillRect(Vector2& outPosition, Vector2& outSize) const
	{
		const float ratio = maxValue_ > kMinMaxValue ? std::clamp(value_ / maxValue_, 0.0f, 1.0f) : 0.0f;

		outPosition = { 0.0f, 0.0f };
		outSize = size_;

		switch (fillDirection_)
		{
		case FillDirection::RightToLeft:
			outSize.x = size_.x * ratio;
			outPosition.x = size_.x - outSize.x;
			break;
		case FillDirection::TopToBottom:
			outSize.y = size_.y * ratio;
			break;
		case FillDirection::BottomToTop:
			outSize.y = size_.y * ratio;
			outPosition.y = size_.y - outSize.y;
			break;
		case FillDirection::LeftToRight:
		default:
			outSize.x = size_.x * ratio;
			break;
		}
	}

	const char* GaugeComponent::FillDirectionToString(FillDirection direction)
	{
		switch (direction)
		{
		case FillDirection::RightToLeft:
			return "RightToLeft";
		case FillDirection::TopToBottom:
			return "TopToBottom";
		case FillDirection::BottomToTop:
			return "BottomToTop";
		case FillDirection::LeftToRight:
		default:
			return "LeftToRight";
		}
	}

	GaugeComponent::FillDirection GaugeComponent::FillDirectionFromString(const std::string& value, FillDirection defaultValue)
	{
		if (value == "LeftToRight")
		{
			return FillDirection::LeftToRight;
		}
		if (value == "RightToLeft")
		{
			return FillDirection::RightToLeft;
		}
		if (value == "TopToBottom")
		{
			return FillDirection::TopToBottom;
		}
		if (value == "BottomToTop")
		{
			return FillDirection::BottomToTop;
		}

		return defaultValue;
	}
}
