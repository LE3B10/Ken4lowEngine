#define NOMINMAX
#include "FontRasterizer.h"

#include "FontFileLoader.h"

#include <Windows.h>
#include <dwrite.h>
#include <iostream>
#include <vector>
#include <algorithm>

FontRasterizer::~FontRasterizer()
{
	if (dwriteFactory_ && fontFileLoader_)
	{
		dwriteFactory_->UnregisterFontFileLoader(fontFileLoader_);
	}

	if (fontFileLoader_)
	{
		fontFileLoader_->Release();
		fontFileLoader_ = nullptr;
	}
}

bool FontRasterizer::Initialize()
{
	if (dwriteFactory_)
	{
		return true;
	}

	HRESULT hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())
	);

	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to create IDWriteFactory.\n";
		return false;
	}

	fontFileLoader_ = new FontFileLoader();
	hr = dwriteFactory_->RegisterFontFileLoader(fontFileLoader_);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to register custom font file loader.\n";
		fontFileLoader_->Release();
		fontFileLoader_ = nullptr;
		return false;
	}

	std::wcout << L"[FontRasterizer] IDWriteFactory created successfully.\n";
	std::wcout << L"[FontRasterizer] Custom font file loader registered.\n";
	return true;
}

bool FontRasterizer::LoadFontFile(const std::wstring& filePath)
{
	if (filePath.empty())
	{
		std::wcerr << L"[FontRasterizer] Font file path is empty.\n";
		return false;
	}

	if (!dwriteFactory_ || !fontFileLoader_)
	{
		std::wcerr << L"[FontRasterizer] Factory or loader is not initialized.\n";
		return false;
	}

	loadedFontPath_ = filePath;

	fontFile_.Reset();
	fontFace_.Reset();

	const void* fontKey = loadedFontPath_.c_str();
	const UINT32 fontKeySize = static_cast<UINT32>((loadedFontPath_.size() + 1) * sizeof(wchar_t));

	HRESULT hr = dwriteFactory_->CreateCustomFontFileReference(
		fontKey,
		fontKeySize,
		fontFileLoader_,
		&fontFile_
	);

	if (FAILED(hr) || !fontFile_)
	{
		std::wcerr << L"[FontRasterizer] Failed to create custom font file reference.\n";
		return false;
	}

	BOOL isSupported = FALSE;
	DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
	DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
	UINT32 numberOfFaces = 0;

	hr = fontFile_->Analyze(&isSupported, &fileType, &faceType, &numberOfFaces);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] fontFile_->Analyze failed.\n";
		return false;
	}

	std::wcout << L"[FontRasterizer] Analyze succeeded. "
		<< L"supported=" << isSupported
		<< L" faces=" << numberOfFaces
		<< L" fileType=" << static_cast<int>(fileType)
		<< L" faceType=" << static_cast<int>(faceType)
		<< L"\n";

	if (!isSupported || numberOfFaces == 0)
	{
		std::wcerr << L"[FontRasterizer] Font file is not supported or has no faces.\n";
		return false;
	}

	IDWriteFontFile* fontFiles[] = { fontFile_.Get() };
	hr = dwriteFactory_->CreateFontFace(
		faceType,
		1,
		fontFiles,
		0,
		DWRITE_FONT_SIMULATIONS_NONE,
		&fontFace_
	);

	if (FAILED(hr) || !fontFace_)
	{
		std::wcerr << L"[FontRasterizer] Failed to create IDWriteFontFace.\n";
		return false;
	}

	std::wcout << L"[FontRasterizer] Font path received: " << loadedFontPath_ << L"\n";
	std::wcout << L"[FontRasterizer] IDWriteFontFace created successfully.\n";

	return true;
}

bool FontRasterizer::HasGlyph(wchar_t character) const
{
	if (!fontFace_)
	{
		return false;
	}

	const UINT32 codepoint = static_cast<UINT32>(character);
	UINT16 glyphIndex = 0;
	fontFace_->GetGlyphIndicesW(&codepoint, 1, &glyphIndex);

	return glyphIndex != 0;
}

RasterizedGlyph FontRasterizer::RasterizeGlyph(wchar_t character, float fontSize) const
{
	if (!dwriteFactory_)
	{
		std::wcerr << L"[FontRasterizer] Factory is not initialized.\n";
		return RasterizedGlyph{};
	}

	if (loadedFontPath_.empty())
	{
		std::wcerr << L"[FontRasterizer] Font file is not loaded.\n";
		return RasterizedGlyph{};
	}

	if (!fontFace_)
	{
		std::wcerr << L"[FontRasterizer] FontFace is not available.\n";
		return RasterizedGlyph{};
	}

	RasterizedGlyph glyph{};

	const UINT32 codepoint = static_cast<UINT32>(character);
	UINT16 glyphIndex = 0;
	fontFace_->GetGlyphIndicesW(&codepoint, 1, &glyphIndex);

	if (glyphIndex == 0)
	{
		std::wcerr << L"[FontRasterizer] Missing glyph: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	DWRITE_FONT_METRICS fontMetrics{};
	fontFace_->GetMetrics(&fontMetrics);

	DWRITE_GLYPH_METRICS glyphMetrics{};
	HRESULT hr = fontFace_->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to get glyph metrics: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	const float scale = fontSize / static_cast<float>(fontMetrics.designUnitsPerEm);

	const float width =
		static_cast<float>(glyphMetrics.advanceWidth - glyphMetrics.leftSideBearing - glyphMetrics.rightSideBearing) * scale;

	const float height =
		static_cast<float>(glyphMetrics.advanceHeight - glyphMetrics.topSideBearing - glyphMetrics.bottomSideBearing) * scale;

	glyph.glyphInfo.codepoint = codepoint;
	glyph.glyphInfo.u0 = 0.0f;
	glyph.glyphInfo.v0 = 0.0f;
	glyph.glyphInfo.u1 = 1.0f;
	glyph.glyphInfo.v1 = 1.0f;

	glyph.glyphInfo.width = (width > 0.0f) ? width : 1.0f;
	glyph.glyphInfo.height = (height > 0.0f) ? height : fontSize;
	glyph.glyphInfo.bearingX = static_cast<float>(glyphMetrics.leftSideBearing) * scale;
	glyph.glyphInfo.bearingY = static_cast<float>(glyphMetrics.verticalOriginY - glyphMetrics.topSideBearing) * scale;
	glyph.glyphInfo.advanceX = static_cast<float>(glyphMetrics.advanceWidth) * scale;

	glyph.bitmapWidth = std::max(1, static_cast<int>(glyph.glyphInfo.width));
	glyph.bitmapHeight = std::max(1, static_cast<int>(glyph.glyphInfo.height));

	if (!BuildGlyphBitmap(glyphIndex, fontSize, glyphMetrics, fontMetrics, glyph))
	{
		std::wcerr << L"[FontRasterizer] Failed to build glyph bitmap: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	glyph.isValid = true;

	std::wcout << L"[FontRasterizer] Rasterized glyph from font: U+"
		<< std::hex << codepoint << std::dec
		<< L" ('" << character << L"')"
		<< L" size=" << fontSize
		<< L" bitmap=" << glyph.bitmapWidth << L"x" << glyph.bitmapHeight << L"\n";

	return glyph;
}

bool FontRasterizer::BuildGlyphBitmap(
	UINT16 glyphIndex,
	float fontSize,
	const DWRITE_GLYPH_METRICS& glyphMetrics,
	const DWRITE_FONT_METRICS& fontMetrics,
	RasterizedGlyph& outGlyph
) const
{
	if (!dwriteFactory_ || !fontFace_)
	{
		return false;
	}

	const float emSize = fontSize;

	UINT16 glyphIndices[1] = { glyphIndex };
	FLOAT glyphAdvances[1] = { 0.0f };
	DWRITE_GLYPH_OFFSET glyphOffsets[1] = {};

	DWRITE_GLYPH_RUN glyphRun{};
	glyphRun.fontFace = fontFace_.Get();
	glyphRun.fontEmSize = emSize;
	glyphRun.glyphCount = 1;
	glyphRun.glyphIndices = glyphIndices;
	glyphRun.glyphAdvances = glyphAdvances;
	glyphRun.glyphOffsets = glyphOffsets;
	glyphRun.isSideways = FALSE;
	glyphRun.bidiLevel = 0;

	Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;

	HRESULT hr = dwriteFactory_->CreateGlyphRunAnalysis(
		&glyphRun,
		1.0f,
		nullptr,
		DWRITE_RENDERING_MODE_ALIASED,
		DWRITE_MEASURING_MODE_NATURAL,
		0.0f,
		0.0f,
		&analysis
	);

	if (FAILED(hr) || !analysis)
	{
		return false;
	}

	RECT textureBounds{};
	hr = analysis->GetAlphaTextureBounds(
		DWRITE_TEXTURE_ALIASED_1x1,
		&textureBounds
	);

	if (FAILED(hr))
	{
		return false;
	}

	const int width = textureBounds.right - textureBounds.left;
	const int height = textureBounds.bottom - textureBounds.top;

	if (width <= 0 || height <= 0)
	{
		outGlyph.bitmapWidth = 1;
		outGlyph.bitmapHeight = 1;
		outGlyph.pixels.assign(1, 0);
		return true;
	}

	std::vector<std::uint8_t> alphaPixels(static_cast<size_t>(width * height), 0);

	hr = analysis->CreateAlphaTexture(
		DWRITE_TEXTURE_ALIASED_1x1,
		&textureBounds,
		alphaPixels.data(),
		static_cast<UINT32>(alphaPixels.size())
	);

	if (FAILED(hr))
	{
		return false;
	}

	outGlyph.bitmapWidth = width;
	outGlyph.bitmapHeight = height;
	outGlyph.pixels = std::move(alphaPixels);

	(void)glyphMetrics;
	(void)fontMetrics;

	return true;
}

RasterizedGlyph FontRasterizer::BuildFallbackGlyph(wchar_t character, float fontSize) const
{
	RasterizedGlyph glyph{};

	glyph.glyphInfo.codepoint = static_cast<std::uint32_t>(character);
	glyph.glyphInfo.u0 = 0.0f;
	glyph.glyphInfo.v0 = 0.0f;
	glyph.glyphInfo.u1 = 1.0f;
	glyph.glyphInfo.v1 = 1.0f;

	glyph.glyphInfo.width = fontSize * 0.5f;
	glyph.glyphInfo.height = fontSize;
	glyph.glyphInfo.bearingX = 0.0f;
	glyph.glyphInfo.bearingY = fontSize;
	glyph.glyphInfo.advanceX = fontSize * 0.6f;

	glyph.bitmapWidth = static_cast<int>(fontSize * 0.5f);
	glyph.bitmapHeight = static_cast<int>(fontSize);

	if (glyph.bitmapWidth < 1)  glyph.bitmapWidth = 1;
	if (glyph.bitmapHeight < 1) glyph.bitmapHeight = 1;

	glyph.pixels.resize(static_cast<size_t>(glyph.bitmapWidth * glyph.bitmapHeight), 255);
	glyph.isValid = true;

	std::wcout << L"[FontRasterizer] Rasterized fallback glyph bitmap: U+"
		<< std::hex << static_cast<std::uint32_t>(character) << std::dec
		<< L" ('" << character << L"')"
		<< L" size=" << fontSize << L"\n";

	return glyph;
}