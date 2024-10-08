#include "DirectXDescriptor.h"

#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

void DirectXDescriptor::Initialize(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2, uint32_t width, uint32_t height)
{
	// RTVディスクイリプタヒープの生成
	descriptorHeaps[static_cast<size_t>(DescriptorType::RTV)] =
		CreateDescriptorHeap(DescriptorType::RTV, device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// DSV用のヒープでディスクリプタの数は１。DSVはShader内で触れるものではないので、ShaderVisibleはfalse
	descriptorHeaps[static_cast<size_t>(DescriptorType::DSV)] =
		CreateDescriptorHeap(DescriptorType::DSV, device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// SRVディスクイリプタヒープの生成
	descriptorHeaps[static_cast<size_t>(DescriptorType::SRV)] =
		CreateDescriptorHeap(DescriptorType::SRV, device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// レンダーターゲットビューの生成
	GenerateRTV(device, swapChainResoursec1, swapChainResoursec2);

	// デプスステンシルビューの生成
	GenerateDSV(device, width, height);

}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXDescriptor::CreateDescriptorHeap(DescriptorType descriptorType, ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible)
{
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap = nullptr;

	//ディスクリプタヒープの生成
	descriptorHeapDescs[static_cast<size_t>(descriptorType)].Type = heapType;	//レンダーターゲットビュー用
	descriptorHeapDescs[static_cast<size_t>(descriptorType)].NumDescriptors = numDescriptors;						//ダブルバッファ用に2つ。多くても別に構わない
	descriptorHeapDescs[static_cast<size_t>(descriptorType)].Flags = shadervisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDescs[static_cast<size_t>(descriptorType)], IID_PPV_ARGS(&descriptorHeap));
	//ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXDescriptor::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height)
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
	Microsoft::WRL::ComPtr <ID3D12Resource> resource = nullptr;
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

void DirectXDescriptor::GenerateRTV(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2)
{
	//RTVの設定
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;		//出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;	//2dテクスチャとして書き込む

	//ディスクリプタの先頭を取得する
	rtvStartHandle = descriptorHeaps[static_cast<size_t>(DescriptorType::RTV)]->GetCPUDescriptorHandleForHeapStart();

	//まず１つ目を作る。１つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResoursec1, &rtvDesc, rtvHandles[0]);

	//2つ目のディスクリプタハンドルを得る（自力で）
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//2つ目を作る
	device->CreateRenderTargetView(swapChainResoursec2, &rtvDesc, rtvHandles[1]);
}

void DirectXDescriptor::GenerateDSV(ID3D12Device* device, uint32_t width, uint32_t height)
{
	//DepthStencilTextureをウィンドウのサイズで作成
	depthStencilResource = CreateDepthStencilTextureResource(device, width, height);

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	//2DTexture

	//DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, descriptorHeaps[static_cast<size_t>(DescriptorType::DSV)]->GetCPUDescriptorHandleForHeapStart());
}

ID3D12DescriptorHeap* DirectXDescriptor::GetDSVDescriptorHeap() const
{
	return descriptorHeaps[static_cast<size_t>(DescriptorType::DSV)].Get();
}

ID3D12DescriptorHeap* DirectXDescriptor::GetSRVDescriptorHeap() const
{
	return descriptorHeaps[static_cast<size_t>(DescriptorType::SRV)].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXDescriptor::GetRTVHandles(uint32_t num)
{
	return rtvHandles[num];
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXDescriptor::GetDSVHandle()
{
	return dsvHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXDescriptor::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXDescriptor::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}
