#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///						リソース管理クラス
/// -------------------------------------------------------------
class ResourceManager
{
public: /// ---------- メンバ関数 ---------- ///

	// Resource作成の関数化
	static ComPtr<ID3D12Resource>CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);
	
};

