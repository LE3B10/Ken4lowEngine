#pragma once
#include "DX12Include.h"


/// -------------------------------------------------------------
///						リソース管理クラス
/// -------------------------------------------------------------
class ResourceManager
{
public: /// ---------- メンバ関数 ---------- ///

	// Resource作成の関数化
	static ComPtr<ID3D12Resource>CreateBufferResource(ID3D12Device* device, size_t size);

	// テクスチャリソース作成の関数化
	static ComPtr<ID3D12Resource>CreateBufferResource(ID3D12Device* device, UINT64 size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState);
};

