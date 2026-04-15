#include "FontFileStream.h"

#include <Windows.h>

#include <fstream>

FontFileStream::FontFileStream(const std::wstring& filePath)
{
	std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
	if (!ifs)
	{
		return;
	}

	const std::streamsize size = ifs.tellg();
	if (size <= 0)
	{
		return;
	}

	ifs.seekg(0, std::ios::beg);

	fileData_.resize(static_cast<size_t>(size));
	ifs.read(reinterpret_cast<char*>(fileData_.data()), size);
}

HRESULT FontFileStream::QueryInterface(REFIID iid, void** ppvObject)
{
	if (ppvObject == nullptr)
	{
		return E_INVALIDARG;
	}

	*ppvObject = nullptr;

	if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileStream))
	{
		*ppvObject = static_cast<IDWriteFontFileStream*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG FontFileStream::AddRef()
{
	return InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_));
}

ULONG FontFileStream::Release()
{
	const ULONG count = InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_));
	if (count == 0)
	{
		delete this;
	}
	return count;
}

HRESULT FontFileStream::ReadFileFragment(
	void const** fragmentStart,
	UINT64 fileOffset,
	UINT64 fragmentSize,
	void** fragmentContext
)
{
	if (fragmentStart == nullptr || fragmentContext == nullptr)
	{
		return E_INVALIDARG;
	}

	*fragmentStart = nullptr;
	*fragmentContext = nullptr;

	if (fileOffset + fragmentSize > fileData_.size())
	{
		return E_FAIL;
	}

	*fragmentStart = fileData_.data() + static_cast<size_t>(fileOffset);
	return S_OK;
}

void FontFileStream::ReleaseFileFragment(void* fragmentContext)
{
	(void)fragmentContext;
}

HRESULT FontFileStream::GetFileSize(UINT64* fileSize)
{
	if (fileSize == nullptr)
	{
		return E_INVALIDARG;
	}

	*fileSize = static_cast<UINT64>(fileData_.size());
	return S_OK;
}

HRESULT FontFileStream::GetLastWriteTime(UINT64* lastWriteTime)
{
	if (lastWriteTime == nullptr)
	{
		return E_INVALIDARG;
	}

	*lastWriteTime = 0;
	return E_NOTIMPL;
}