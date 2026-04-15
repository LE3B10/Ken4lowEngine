#pragma once
#include <cstdint>
#include <vector>

struct GlyphInfo
{
	uint32_t codepoint = 0;

	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 0.0f;
	float v1 = 0.0f;

	float width = 0.0f;
	float height = 0.0f;

	float bearingX = 0.0f;
	float bearingY = 0.0f;
	float advanceX = 0.0f;
};

struct RasterizedGlyph
{
	GlyphInfo glyphInfo;
	std::vector<std::uint8_t> pixels;

	int bitmapWidth = 0;
	int bitmapHeight = 0;

	int atlasX = 0;
	int atlasY = 0;

	bool isValid = false;
};

struct FontAtlas
{
	int width = 0;
	int height = 0;
	std::vector<std::uint8_t> pixels;
	std::vector<RasterizedGlyph> glyphs;

	bool isValid = false;
};