#pragma once

#include <dwrite.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

class FontFileStream : public IDWriteFontFileStream
{
public:
    explicit FontFileStream(const std::wstring& filePath);

    bool IsLoaded() const { return !fileData_.empty(); }

    IFACEMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;
    IFACEMETHOD_(ULONG, AddRef)() override;
    IFACEMETHOD_(ULONG, Release)() override;

    IFACEMETHOD(ReadFileFragment)(
        void const** fragmentStart,
        UINT64 fileOffset,
        UINT64 fragmentSize,
        void** fragmentContext
        ) override;

    IFACEMETHOD_(void, ReleaseFileFragment)(
        void* fragmentContext
        ) override;

    IFACEMETHOD(GetFileSize)(
        UINT64* fileSize
        ) override;

    IFACEMETHOD(GetLastWriteTime)(
        UINT64* lastWriteTime
        ) override;

private:
    ~FontFileStream() = default;

private:
    ULONG refCount_ = 1;
    std::vector<std::uint8_t> fileData_;
};