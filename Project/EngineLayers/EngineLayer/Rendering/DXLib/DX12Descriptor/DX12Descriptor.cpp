#include "DX12Descriptor.h"

#include "SRVManager.h"

#include <cassert>
#include <format>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


/// -------------------------------------------------------------
///				各種のデスクリプタヒープの生成
/// -------------------------------------------------------------
void DX12Descriptor::Initialize(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2, uint32_t width, uint32_t height)
{
	device_ = device;

	// デスクリプタサイズの設定
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTVディスクイリプタヒープの生成
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// DSV用のヒープでディスクリプタの数は１。DSVはShader内で触れるものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// レンダーターゲットビューの生成
	GenerateRTV(device_, { swapChainResoursec1, swapChainResoursec2 });

	// デプスステンシルビューの生成
	GenerateDSV(device_, width, height);

}


/// -------------------------------------------------------------
///				デスクリプタヒープを生成する
/// -------------------------------------------------------------
ComPtr<ID3D12DescriptorHeap> DX12Descriptor::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible)
{
	ComPtr <ID3D12DescriptorHeap> descriptorHeap = nullptr;

	//ディスクリプタヒープの生成
	descriptorHeapDesc.Type = heapType;	//レンダーターゲットビュー用
	descriptorHeapDesc.NumDescriptors = numDescriptors;						//ダブルバッファ用に2つ。多くても別に構わない
	descriptorHeapDesc.Flags = shadervisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	//ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}


/// -------------------------------------------------------------
///				深度ステンシルテクスチャの作成
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> DX12Descriptor::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height)
{
	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;										// Textureの幅
	resourceDesc.Height = height;									// Textureの高さ
	resourceDesc.MipLevels = 1;										// mipmapの数
	resourceDesc.DepthOrArraySize = 1;								// 奥行 or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			// DepthStencilとして知用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;								// サンプリングカウント。１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// ２次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapPropaties{};
	heapPropaties.Type = D3D12_HEAP_TYPE_DEFAULT;					//VRAMに作る

	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;					//1.0f（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;		//フォーマット。Resourceと合わせる

	//Resourceの生成
	ComPtr <ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapPropaties,							// Heapの設定
		D3D12_HEAP_FLAG_NONE,					// Heapの特殊な設定。特になし。
		&resourceDesc,							// Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,		// 深度値を書き込む状態にしておく
		&depthClearValue,						// Clear最適地
		IID_PPV_ARGS(&resource));				// 作成するResourceポインタのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}


/// -------------------------------------------------------------
///				レンダーターゲットビューの生成
/// -------------------------------------------------------------
void DX12Descriptor::GenerateRTV(ID3D12Device* device, std::vector<ID3D12Resource*> resources)
{
	// RTV のフォーマットと設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// rtvHandles[] をクリアしておく
	for (auto& handle : rtvHandles) {
		handle.ptr = 0;
	}

	// ディスクリプタの開始位置を取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// 各リソースに対して RTV を作成
	for (size_t i = 0; i < resources.size(); ++i) {
		assert(i < _countof(rtvHandles)); // 範囲チェック

		// 現在の RTV ハンドルを保存
		rtvHandles[i] = rtvHandle;

		// RTV を作成
		device->CreateRenderTargetView(resources[i], &rtvDesc, rtvHandle);

		// ディスクリプタのサイズ分ハンドルを進める
		rtvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
}


/// -------------------------------------------------------------
///				デプスステンシルビューの生成
/// -------------------------------------------------------------
void DX12Descriptor::GenerateDSV(ID3D12Device* device, uint32_t width, uint32_t height)
{
	// DepthStencilTextureをウィンドウのサイズで作成
	depthStencilResource = CreateDepthStencilTextureResource(device, width, height);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	//2DTexture

	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}


/// -------------------------------------------------------------
///					RTVの2Dテクスチャの処理
/// -------------------------------------------------------------
void DX12Descriptor::CreateRTVForTexture2D(ID3D12Device* device, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = resource->GetDesc().Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	device->CreateRenderTargetView(resource, &rtvDesc, rtvHandle);
}


/// -------------------------------------------------------------
///					デプスステンシルビューの生成
/// -------------------------------------------------------------
void DX12Descriptor::CreateDSVForDepthBuffer(ID3D12Device* device, ID3D12Resource* depthResource, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	device->CreateDepthStencilView(depthResource, &dsvDesc, dsvHandle);
}


D3D12_CPU_DESCRIPTOR_HANDLE DX12Descriptor::GetFreeRTVHandle()
{
	assert(descriptorSizeRTV > 0);

	uint32_t usedCount = 0;

	// 有効な RTV ハンドルをカウント
	for (const auto& handle : rtvHandles)
	{
		if (handle.ptr != 0)
		{
			++usedCount;
		}
	}

	// `usedCount` が配列サイズを超えていないか確認
	assert(usedCount < _countof(rtvHandles));

	// 次に使える空きハンドルを計算
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += descriptorSizeRTV * usedCount;
	return handle;
}