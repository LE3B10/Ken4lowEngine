#include "TextManager.h"
#include <cassert>

namespace Ken4lowEngine
{
	TextManager* TextManager::GetInstance()
	{
		static TextManager instance;
		return &instance;
	}

	void TextManager::Initialize()
	{
		Finalize();

		// 英数字と基本記号は共通のテクスチャと定義でまとめておく
		RegisterFontFromJson(
			"latin",
			"UI/Font/Latin/minecraft_atlas.dds",
			"Resources/Fonts/Compiled/Latin/minecraft.json",
			32.0f,
			32.0f,
			U'？'
		);

		// 現状は日本語フォントと同じものをUI用にも登録しているが、必要に応じて別のフォントを登録してもいい
		RegisterFontFromJson(
			"jp",
			"UI/Font/JP/PixelMplus12-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/PixelMplus12-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
	}

	void TextManager::Finalize()
	{
		for (auto& [key, drawer] : drawers_)
		{
			if (drawer)
			{
				drawer->Finalize();
			}
		}
		drawers_.clear();
	}

	void TextManager::BeginFrame()
	{
		for (auto& [key, drawer] : drawers_)
		{
			if (drawer)
			{
				drawer->Reset();
			}
		}
	}

	void TextManager::RegisterFontFromJson(const std::string& fontKey, const std::string& texturePath, const std::string& jsonPath, float baseDrawWidth, float baseDrawHeight, char32_t fallbackCodepoint)
	{
		auto fontDef = FontAtlasLoader::LoadFromJson(texturePath, jsonPath, baseDrawWidth, baseDrawHeight, fallbackCodepoint);

		auto drawer = std::make_unique<TextSpriteDrawer>();
		drawer->Initialize(fontDef);
		drawers_[fontKey] = std::move(drawer);
	}

	void TextManager::RegisterFontFromAtlasText(const std::string& fontKey, const std::string& texturePath, const std::string& atlasPath, float baseDrawWidth, float baseDrawHeight, char32_t fallbackCodepoint)
	{
		auto fontDef = FontAtlasLoader::LoadFromAtlasText(texturePath, atlasPath, baseDrawWidth, baseDrawHeight, fallbackCodepoint);

		auto drawer = std::make_unique<TextSpriteDrawer>();
		drawer->Initialize(fontDef);
		drawers_[fontKey] = std::move(drawer);
	}

	TextSpriteDrawer* TextManager::FindDrawer(const std::string& fontKey)
	{
		auto it = drawers_.find(fontKey);
		if (it == drawers_.end())
		{
			return nullptr;
		}
		return it->second.get();
	}

	void TextManager::Draw(const std::string& fontKey, const std::string& utf8Text, const Vector2& position, const TextStyle& style)
	{
		TextSpriteDrawer* drawer = FindDrawer(fontKey);
		assert(drawer && "TextManager::Draw fontKey not found");

		if (!drawer)
		{
			return;
		}

		drawer->SetColor(style.color);
		drawer->SetScale(style.scale);
		drawer->SetLetterSpacing(style.letterSpacing);
		drawer->SetLineSpacing(style.lineSpacing);

		switch (style.align)
		{
		case TextAlign::Left:
			drawer->DrawTextLeftAligned(utf8Text, position);
			break;
		case TextAlign::Center:
			drawer->DrawTextCentered(utf8Text, position);
			break;
		case TextAlign::Right:
			drawer->DrawTextRightAligned(utf8Text, position);
			break;
		}
	}

	float TextManager::MeasureWidth(const std::string& fontKey, const std::string& utf8Text, float scale, float letterSpacing)
	{
		TextSpriteDrawer* drawer = FindDrawer(fontKey);
		if (!drawer)
		{
			return 0.0f;
		}

		drawer->SetScale(scale);
		drawer->SetLetterSpacing(letterSpacing);
		return drawer->MeasureWidth(utf8Text);
	}
}