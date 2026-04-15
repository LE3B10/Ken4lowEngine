#pragma once
#include <string>
#include <wrl/client.h>
#include <dwrite.h>

#include "FontConverterTypes.h"

class FontFileLoader;

class FontRasterizer
{
public:
	FontRasterizer() = default;
	~FontRasterizer();

	bool Initialize();
	bool LoadFontFile(const std::wstring& filePath);

	bool HasGlyph(wchar_t character) const;
	RasterizedGlyph RasterizeGlyph(wchar_t character, float fontSize) const;

private:
	RasterizedGlyph BuildFallbackGlyph(wchar_t character, float fontSize) const;
	bool BuildGlyphBitmap(
		UINT16 glyphIndex,
		float fontSize,
		const DWRITE_GLYPH_METRICS& glyphMetrics,
		const DWRITE_FONT_METRICS& fontMetrics,
		RasterizedGlyph& outGlyph
	) const;

private:
	Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
	std::wstring loadedFontPath_;

	IDWriteFontFileLoader* fontFileLoader_ = nullptr;
	Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile_;
	Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace_;
};