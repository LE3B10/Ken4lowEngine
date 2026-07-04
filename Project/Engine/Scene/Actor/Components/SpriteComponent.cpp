#include "SpriteComponent.h"

#include "AssetPathSelector.h"
#include "Sprite.h"
#include "SpriteManager.h"

#include <array>
#include <cstdio>
#include <memory>

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

	SpriteComponent::~SpriteComponent() = default;

	void SpriteComponent::Initialize()
	{
		EnsureSprite();
		ApplySpriteSettings();
	}

	void SpriteComponent::Update([[maybe_unused]] float deltaTime)
	{
		EnsureSprite();
		ApplySpriteSettings();
	}

	void SpriteComponent::Draw()
	{
		// Screen Space SpriteはActorWorldの2D描画パスでまとめて描画する
	}

	void SpriteComponent::DrawScreenSpace()
	{
		if (!visible_ || texturePath_.empty())
		{
			return; // 非表示またはTexture未設定の場合は描画しない
		}

		if (!sprite_)
		{
			return; // Spriteが生成できない場合は描画しない
		}

		ApplySpriteSettings();
		SpriteManager::GetInstance()->SetRenderSetting_UI();
		sprite_->Draw();
	}

	bool SpriteComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_ && !texturePath_.empty();
	}

	void SpriteComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("スプライトコンポーネント");

		std::array<char, 256> texturePathBuffer{};
		std::snprintf(texturePathBuffer.data(), texturePathBuffer.size(), "%s", texturePath_.c_str());
		if (ImGui::InputText("テクスチャパス", texturePathBuffer.data(), texturePathBuffer.size()))
		{
			SetTexturePath(texturePathBuffer.data());
		}

		std::string selectedTexturePath = texturePath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##SpriteComponentTexturePath", selectedTexturePath, AssetType::Texture))
		{
			SetTexturePath(selectedTexturePath);
		}

		ImGui::DragFloat2("位置", &position_.x, 1.0f);
		ImGui::DragFloat2("サイズ", &size_.x, 1.0f, 0.0f, 4096.0f);
		ImGui::ColorEdit4("色", &color_.x);
		ImGui::Checkbox("表示", &visible_);
		ImGui::DragFloat("回転", &rotation_, 0.01f);
		ImGui::DragFloat2("アンカー", &anchor_.x, 0.01f, 0.0f, 1.0f);
#endif // USE_IMGUI
	}

	void SpriteComponent::Finalize()
	{
		if (sprite_)
		{
			sprite_->Finalize(); // Component破棄時にSpriteリソースを解放する
			sprite_.reset();
		}

		loadedTexturePath_.clear();
	}

	void SpriteComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // SpriteComponentとして保存する
		outJson["TexturePath"] = texturePath_;
		outJson["Position"] = { position_.x, position_.y };
		outJson["Size"] = { size_.x, size_.y };
		outJson["Color"] = { color_.x, color_.y, color_.z, color_.w };
		outJson["Visible"] = visible_;
		outJson["Rotation"] = rotation_;
		outJson["Anchor"] = { anchor_.x, anchor_.y };
	}

	void SpriteComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

		if (inJson.contains("TexturePath") && inJson["TexturePath"].is_string())
		{
			SetTexturePath(inJson["TexturePath"].get<std::string>()); // SpriteのTextureパスを復元する
		}

		position_ = ReadVector2FromJson(inJson, "Position", position_);
		size_ = ReadVector2FromJson(inJson, "Size", size_);
		color_ = ReadVector4FromJson(inJson, "Color", color_);
		anchor_ = ReadVector2FromJson(inJson, "Anchor", anchor_);

		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>(); // Spriteの表示状態を復元する
		}

		if (inJson.contains("Rotation") && inJson["Rotation"].is_number())
		{
			rotation_ = inJson["Rotation"].get<float>(); // Spriteの回転を復元する
		}

		ApplySpriteSettings();
	}

	void SpriteComponent::SetTexturePath(const std::string& texturePath)
	{
		if (texturePath_ == texturePath)
		{
			return; // 同じTextureなら再生成しない
		}

		texturePath_ = texturePath;
		if (sprite_)
		{
			sprite_->Finalize();
			sprite_.reset();
			loadedTexturePath_.clear();
		}
	}

	void SpriteComponent::EnsureSprite()
	{
		if (texturePath_.empty() || (sprite_ && loadedTexturePath_ == texturePath_))
		{
			return; // Texture未設定、または既に同じTextureで生成済みの場合は何もしない
		}

		if (sprite_)
		{
			sprite_->Finalize();
			sprite_.reset();
		}

		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(texturePath_);
		loadedTexturePath_ = texturePath_;
	}

	void SpriteComponent::ApplySpriteSettings()
	{
		if (!sprite_)
		{
			return; // Sprite未生成の場合は反映しない
		}

		sprite_->SetPosition(position_);
		sprite_->SetSize(size_);
		sprite_->SetColor(color_);
		sprite_->SetRotation(rotation_);
		sprite_->SetAnchorPoint(anchor_);
		sprite_->Update();
	}
}
