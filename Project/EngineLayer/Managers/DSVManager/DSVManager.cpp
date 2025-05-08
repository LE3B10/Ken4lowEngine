#include "DSVManager.h"
#include "DirectXCommon.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


/// -------------------------------------------------------------
///				　シングルトンインスタンスの取得
/// -------------------------------------------------------------
DSVManager* DSVManager::GetInstance()
{
	static DSVManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///					　DSVマネージャの初期化
/// -------------------------------------------------------------
void DSVManager::Initialize(DirectXCommon* dxCommon, uint32_t maxDSVCount)
{
	dxCommon_ = dxCommon;
	maxDSVCount_ = maxDSVCount;

	// DSV用のデスクリプタヒープを作成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.NumDescriptors = maxDSVCount_;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
	if (FAILED(result))
	{
		throw std::runtime_error("Failed to create DSV descriptor heap");
	}

	// DSVのデスクリプタサイズを取得
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}


/// -------------------------------------------------------------
///				DSVを確保（未使用のインデックスを取得）
/// -------------------------------------------------------------
uint32_t DSVManager::Allocate()
{
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// 解放済みのインデックスがある場合は再利用
	if (!freeIndices_.empty())
	{
		uint32_t index = freeIndices_.front();
		freeIndices_.pop();
		return index;
	}

	// 新しく確保するインデックス
	if (useIndex_ >= maxDSVCount_)
	{
		throw std::runtime_error("No more DSV descriptors can be allocated");
	}

	return useIndex_++;
}


/// -------------------------------------------------------------
///			 DSVを解放（再利用可能なインデックスを保存）
/// -------------------------------------------------------------
void DSVManager::Free(uint32_t dsvIndex)
{
	std::lock_guard<std::mutex> lock(allocationMutex_);

	// インデックスの範囲チェック
	if (dsvIndex >= maxDSVCount_)
	{
		throw std::runtime_error("Invalid DSV index for freeing");
	}
}


/// -------------------------------------------------------------
///		指定リソースのDSVを作成（デプスバッファ用）
/// -------------------------------------------------------------
void DSVManager::CreateDSVForDepthBuffer(uint32_t dsvIndex, ID3D12Resource* depthResource)
{
	assert(depthResource && "Depth buffer resource is null!");

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	// DSVを作成
	dxCommon_->GetDevice()->CreateDepthStencilView(depthResource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
}

void DSVManager::CreateDSVForTexture2D(uint32_t dsvIndex, ID3D12Resource* resource)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	dxCommon_->GetDevice()->CreateDepthStencilView(resource, &dsvDesc, GetCPUDescriptorHandle(dsvIndex));
}


/// -------------------------------------------------------------
///			指定インデックスのCPUデスクリプタハンドルを取得
/// -------------------------------------------------------------
D3D12_CPU_DESCRIPTOR_HANDLE DSVManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize_ * index;
	return handle;
}
