#include "DX12SwapChain.h"
#include "WinApp.h"

#include <cassert>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///				スワップチェインの初期化処理
/// -------------------------------------------------------------
void DX12SwapChain::Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, uint32_t Width, uint32_t Height)
{
	HRESULT hr{};

	swapChain = nullptr;

	//スワップチェーンを生成する
	swapChainDesc.Width = Width;									// 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = Height;									// 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
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
	swapChainResources[0]->SetName(L"BackBuffer : 0");
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));

	// 2つ目のバッファも同様に取得
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	swapChainResources[1]->SetName(L"BackBuffer : 1");
	assert(SUCCEEDED(hr));
}

void DX12SwapChain::Resize(uint32_t width, uint32_t height)
{
	if (!swapChain) return;

	// 既存参照を外す（重要）
	for (auto& r : swapChainResources) r.Reset();

	// 0 / UNKNOWN で「現状維持」
	HRESULT hr = swapChain->ResizeBuffers(
		0, width, height,
		DXGI_FORMAT_UNKNOWN,
		0
	);
	assert(SUCCEEDED(hr));

	// 再取得
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// descも更新しておくと便利
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
}

} // namespace Ken4lowEngine
