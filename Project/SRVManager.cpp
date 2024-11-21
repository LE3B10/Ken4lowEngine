#include "SRVManager.h"

#include "DirectXCommon.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// 最大SRV数（最大テクスチャ枚数）
const uint32_t SRVManager::kMaxSRVCount = 512;


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void SRVManager::Initialize(DirectXCommon* dxCommon)
{
	// 引数を受け取ってメンバ変数に代入する
	dxCommon_ = dxCommon;

	// デスクリプタヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = kMaxSRVCount;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	// デスクリプタヒープの作成
	HRESULT result = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	if (FAILED(result)) {
		throw std::runtime_error("Failed to create SRV descriptor heap");
	}

	// デスクリプタサイズを取得
	descriptorSize = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void SRVManager::CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels)
{
	assert(pResource != nullptr && "pResource is null");

	// srvDescの項目を埋める
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format; // テクスチャのフォーマット
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // デフォルトのマッピング
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;				//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = MipLevels;
	// Shader Resource View を作成
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SRVManager::CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	assert(pResource != nullptr && "pResource is null");

	// SRV 設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造化バッファではフォーマットは UNKNOWN
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER; // バッファとして扱う
	srvDesc.Buffer.FirstElement = 0;                   // バッファの先頭要素から開始
	srvDesc.Buffer.NumElements = numElements;          // バッファの要素数
	srvDesc.Buffer.StructureByteStride = structureByteStride; // 各要素のサイズ（バイト単位）
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE; // 特殊なフラグなし

	// Shader Resource View を作成
	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

void SRVManager::PreDraw()
{
	// ディスクリプタヒープの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

void SRVManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

uint32_t SRVManager::Allocate()
{
	// 上限に達してないかチェック
	if (useIndex >= kMaxSRVCount)
	{
		assert(false && "No more SRV descriptors can be allocated");
		return UINT32_MAX; // エラー値を返す
	}

	// return する番号を一旦記録する
	int index = useIndex;

	// 次回のために番号を1進める
	useIndex++;

	// 割り当てたインデックスを返す
	return index;
}

ID3D12DescriptorHeap* SRVManager::GetDescriptorHeap() const
{
	return descriptorHeap.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SRVManager::GetCPUDescriptorHandle(UINT index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE SRVManager::GetGPUDescriptorHandle(UINT index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSize * index;
	return handle;
}
