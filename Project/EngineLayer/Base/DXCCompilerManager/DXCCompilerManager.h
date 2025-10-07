#pragma once
#include <DX12Include.h>

class DXCCompilerManager
{
public:
	void Initialize();
	IDxcUtils* GetIDxcUtils() const { return dxcUtils_.Get(); }
	IDxcCompiler3* GetIDxcCompiler() const { return dxcCompiler_.Get(); }
	IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler_.Get(); }

private:
	ComPtr<IDxcUtils> dxcUtils_;
	ComPtr<IDxcCompiler3> dxcCompiler_;
	ComPtr<IDxcIncludeHandler> includeHandler_;
};