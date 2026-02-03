#include "ResourceManager.h"

#include "DirectXCommon.h"

namespace Ken4lowEngine
{

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

/// -------------------------------------------------------------
///					バッファリソースの生成
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> ResourceManager::CreateBufferResource(ID3D12Device* device, size_t size)
{
	return CreateBufferResource(device, size,
		D3D12_HEAP_TYPE_UPLOAD,				// アップロード用ヒープ
		D3D12_RESOURCE_FLAG_NONE,			// バッファ用なのでフラグ無し
		D3D12_RESOURCE_STATE_GENERIC_READ	// アップロード用は GENERIC_READ ステートで作成
	);
}

/// -------------------------------------------------------------
///					バッファリソースの生成（詳細指定版）
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> ResourceManager::CreateBufferResource(ID3D12Device* device, UINT64 size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState)
{
	//頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = type;	//UploadHeapを使う

	//頂点リソースの設定
	D3D12_RESOURCE_DESC desc{};

	//バッファリソース。テクスチャの場合はまた別の設定をする
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = size;				// リソースのサイズ
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = flags;

	//実際に頂点リソースを作る
	ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = S_FALSE;
	hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initState, nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

} // namespace Ken4lowEngine
