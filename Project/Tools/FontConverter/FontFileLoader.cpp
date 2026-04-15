#include "FontFileLoader.h"
#include "FontFileStream.h"

#include <Windows.h>

#include <string>

HRESULT FontFileLoader::QueryInterface(REFIID iid, void** ppvObject)
{
	if (ppvObject == nullptr)
	{
		return E_INVALIDARG;
	}

	*ppvObject = nullptr;

	if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileLoader))
	{
		*ppvObject = static_cast<IDWriteFontFileLoader*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG FontFileLoader::AddRef()
{
	return InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_));
}

ULONG FontFileLoader::Release()
{
	const ULONG count = InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_));
	if (count == 0)
	{
		delete this;
	}
	return count;
}

HRESULT FontFileLoader::CreateStreamFromKey(
	void const* fontFileReferenceKey,
	UINT32 fontFileReferenceKeySize,
	IDWriteFontFileStream** fontFileStream
)
{
	if (fontFileReferenceKey == nullptr || fontFileStream == nullptr)
	{
		return E_INVALIDARG;
	}

	*fontFileStream = nullptr;

	if (fontFileReferenceKeySize < sizeof(wchar_t) || (fontFileReferenceKeySize % sizeof(wchar_t)) != 0)
	{
		return E_INVALIDARG;
	}

	const wchar_t* keyChars = static_cast<const wchar_t*>(fontFileReferenceKey);
	const size_t charCount = (fontFileReferenceKeySize / sizeof(wchar_t)) - 1;

	std::wstring filePath(keyChars, keyChars + charCount);

	FontFileStream* stream = new FontFileStream(filePath);
	if (!stream->IsLoaded())
	{
		stream->Release();
		return E_FAIL;
	}

	*fontFileStream = stream;
	return S_OK;
}