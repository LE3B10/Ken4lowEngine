#pragma once
#include <Windows.h>

#include <cstdint>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

/// ---------- 前方宣言 ---------- ///
class WinApp;

/// -------------------------------------------------------------
///				スワップチェインの生成クラス
/// -------------------------------------------------------------
class DX12SwapChain
{
public: /// ---------- メンバ関数 ---------- ///

	// スワップチェインの生成
	void Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, uint32_t Width, uint32_t Height);

public: /// ---------- ゲッター ---------- ///

	// スワップチェインの取得
	IDXGISwapChain4* GetSwapChain() const { return swapChain.Get(); }

	// スワップチェインのリソースを取得
	ID3D12Resource* GetSwapChainResources(uint32_t num) const { return swapChainResources[num].Get(); }

	// スワップチェインの情報を取得
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() { return swapChainDesc; }

	// スワップチェインの現在のバッファの状態を取得
	D3D12_RESOURCE_STATES GetBackBufferState(uint32_t index) { return backBufferStates[index]; }

public: /// ---------- セッター ---------- ///

	// スワップチェインの現在のバッファの状態を設定
	void SetBackBufferState(uint32_t index, D3D12_RESOURCE_STATES state) { backBufferStates[index] = state; }

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain;
	Microsoft::WRL::ComPtr <ID3D12Resource> swapChainResources[2];
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	D3D12_RESOURCE_STATES backBufferStates[2];  // 2つのバックバッファの状態を記録
};

