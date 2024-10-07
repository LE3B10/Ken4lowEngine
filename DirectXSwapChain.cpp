#include "DirectXSwapChain.h"

#include <cassert>

#include "WinApp.h"

void DirectXSwapChain::Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue)
{
	HRESULT hr{};

	swapChain = nullptr;

	//スワップチェーンを生成する
	swapChainDesc.Width = WinApp::kClientWidth;						// 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = WinApp::kClientHeight;					// 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				// 色の形式
	swapChainDesc.SampleDesc.Count = 1;								// マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;									// ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		// モニタにうつしたら、中身を破壊

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	//SwapChainからResourceを引っ張ってくる
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));
}

IDXGISwapChain4* DirectXSwapChain::GetSwapChain() const
{
	return swapChain.Get();
}

ID3D12Resource* DirectXSwapChain::GetSwapChainResources(uint32_t num) const
{
	return swapChainResources[num].Get();
}

DXGI_SWAP_CHAIN_DESC1& DirectXSwapChain::GetSwapChainDesc()
{
	// TODO: return ステートメントをここに挿入します
	return swapChainDesc;
}
