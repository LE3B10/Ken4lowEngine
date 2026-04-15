#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "TextSpriteDrawer.h"
#include "FontAtlasLoader.h"
#include "Vector2.h"
#include "Vector4.h"

namespace Ken4lowEngine
{
	enum class TextAlign
	{
		Left,
		Center,
		Right
	};

	struct TextStyle
	{
		TextAlign align = TextAlign::Left;
		float scale = 1.0f;
		float letterSpacing = 0.0f;
		float lineSpacing = 0.0f;
		Vector4 color{ 1.0f,1.0f,1.0f,1.0f };
	};

	class TextManager
	{
	public:
		static TextManager* GetInstance();

		void Initialize();
		void Finalize();

		// 毎フレーム最初に呼ぶ
		void BeginFrame();

		// 必要なら個別フォント追加用
		void RegisterFontFromJson(
			const std::string& fontKey,
			const std::string& texturePath,
			const std::string& jsonPath,
			float baseDrawWidth = 0.0f,
			float baseDrawHeight = 0.0f,
			char32_t fallbackCodepoint = U'?');

		void RegisterFontFromAtlasText(
			const std::string& fontKey,
			const std::string& texturePath,
			const std::string& atlasPath,
			float baseDrawWidth = 0.0f,
			float baseDrawHeight = 0.0f,
			char32_t fallbackCodepoint = U'?');

		void Draw(
			const std::string& fontKey,
			const std::string& utf8Text,
			const Vector2& position,
			const TextStyle& style = {});

		float MeasureWidth(
			const std::string& fontKey,
			const std::string& utf8Text,
			float scale = 1.0f,
			float letterSpacing = 0.0f);

	private:
		TextManager() = default;
		TextManager(const TextManager&) = delete;
		TextManager& operator=(const TextManager&) = delete;

		TextSpriteDrawer* FindDrawer(const std::string& fontKey);

	private:
		std::unordered_map<std::string, std::unique_ptr<TextSpriteDrawer>> drawers_;
	};
}