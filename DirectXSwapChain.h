#pragma once
#include <Windows.h>

#include <cstdint>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

// 前方宣言
class WinApp;

class DirectXSwapChain
{
public: // メンバ関数

	// スワップチェインの生成
	void Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue);

	// ゲッター
	IDXGISwapChain4* GetSwapChain() const;
	ID3D12Resource* GetSwapChainResources(uint32_t num) const;
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc();

private: // メンバ変数
	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain;
	Microsoft::WRL::ComPtr <ID3D12Resource> swapChainResources[2];
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
};

