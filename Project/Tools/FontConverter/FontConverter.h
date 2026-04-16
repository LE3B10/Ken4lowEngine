#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "FontRasterizer.h"

class FontConverter
{
public:
	static void OutputUsage();

	bool ConvertFont(const std::wstring& filePath, int numOptions, wchar_t** options);

private:
	void ParseOptions(int numOptions, wchar_t** options);
	void BuildDefaultCharset();

	bool LoadCharsetFromFile(const std::wstring& filePath);
	std::wstring ReadTextFileUtf8(const std::wstring& filePath) const;

	void SaveMetadataJson(
		const std::wstring& outputPath,
		const std::wstring& inputFilePath,
		const FontAtlas& atlas
	) const;

	void WriteDummyOutput(const std::wstring& outputPath, const std::wstring& inputFilePath) const;
	void SaveGlyphBitmapAsPgm(const std::wstring& outputPath, const RasterizedGlyph& glyph) const;
	void SaveAtlasAsPgm(const std::wstring& outputPath, const FontAtlas& atlas) const;

	void SaveAlphaImageAsPng(
		const std::wstring& outputPath,
		int width,
		int height,
		const std::vector<std::uint8_t>& alphaPixels
	) const;

	std::wstring MakeOutputBaseName(const std::wstring& inputFilePath) const;
	std::string NarrowForJson(const std::wstring& text) const;

	std::vector<RasterizedGlyph> RasterizeCharset() const;
	FontAtlas BuildSimpleHorizontalAtlas(std::vector<RasterizedGlyph> glyphs) const;

private:
	int fontSize_ = 48;
	int atlasWidth_ = 1024;
	int atlasHeight_ = 1024;
	std::wstring outputDirectory_ = L"Resources/Fonts/Compiled/";
	std::wstring charset_;
	std::wstring fallbackFontPath_;

	FontRasterizer fontRasterizer_;
	FontRasterizer fallbackFontRasterizer_;
};