#pragma once
#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "WinApp.h"

// DirectXの基盤
class DirectXCommon
{
public: // メンバ関数
	// 初期化処理
	void Initialize(WinApp* winApp);

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

private: // メンバ関数
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

	// CPUのDescriptorHandleを取得する関数
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	// GPUのDescriptorHandleを取得する関数
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	// デバイスの生成
	void InitializeDevice();
	// コマンド関連の初期化
	void InitializeCommands();
	// スワップチェーンの生成
	void CreateSwapChain();
	// 深度バッファの生成
	void GenerateDepthBuffer();
	// 各種のでスクリプタヒープの生成
	void GenerateDescriptorHeaps();
	// レンダ―ターゲットビューの初期化
	void InitializeRenderTargets();
	// 深度ステンシルビューの初期化
	void InitializeDepthStencilView();
	// フェンスの生成
	void CreateFence();
	// ビューポート矩形の初期化
	void InitializeViewport();
	// シザリング矩形の初期化
	void InitializeScissoring();
	// DXCコンパイラの生成
	void CreateDXCCompiler();
	// ImGuiの初期化
	void InitializeUI();

private: // メンバ変数
	// WindowsAPI
	WinApp* winApp = nullptr;

	Microsoft::WRL::ComPtr <ID3D12Device> device;
	Microsoft::WRL::ComPtr <IDXGIFactory7> dxgiFactory;

	Microsoft::WRL::ComPtr <ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> commandList;

	Microsoft::WRL::ComPtr <ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> rtvDescriptorHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> srvDescriptorHeap;
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap;
	uint32_t descriptorSizeRTV;
	uint32_t descriptorSizeSRV;
	uint32_t descriptorSizeDSV;

	std::array<Microsoft::WRL::ComPtr <ID3D12Resource>, 2> swapChainResources;
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	//RTVを2つ作るのでディスクリプタを２つ用意
	std::array <D3D12_CPU_DESCRIPTOR_HANDLE, 2> rtvHandles;
	static const UINT bufferCount = 2;

	Microsoft::WRL::ComPtr <ID3D12Fence> fence;
	UINT64 fenceValue = 0;

	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	Microsoft::WRL::ComPtr <IDxcUtils> dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	Microsoft::WRL::ComPtr <IDxcIncludeHandler> includeHandler = nullptr;
};
