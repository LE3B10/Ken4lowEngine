#pragma once

#include <dwrite.h>

class FontFileLoader : public IDWriteFontFileLoader
{
public:
	FontFileLoader() = default;

	// IUnknown
	IFACEMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;
	IFACEMETHOD_(ULONG, AddRef)() override;
	IFACEMETHOD_(ULONG, Release)() override;

	// IDWriteFontFileLoader
	IFACEMETHOD(CreateStreamFromKey)(
		void const* fontFileReferenceKey,
		UINT32 fontFileReferenceKeySize,
		IDWriteFontFileStream** fontFileStream
		) override;

private:
	~FontFileLoader() = default;

private:
	ULONG refCount_ = 1;
};