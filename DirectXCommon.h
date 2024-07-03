#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <dxcapi.h>

#include "WinApp.h"

// DirectX基盤
class DirectXCommon
{
public: // メンバ変数
	// 初期化処理
	void Initialize(WinApp* winApp);
	// 描画前処理
	void PreDraw();
	// 描画後処理
	void PostDraw();

public:
	/// <summary>
	/// SRVの指定番号のCPUデスクリプタハンドルを取得
	/// </summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVCPUDescriptorHandle(uint32_t index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVCPUDescriptorHandle(uint32_t index);
	/// <summary>
	/// SRVの指定番号のGPUデスクリプタハンドルを取得
	/// </summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetRTVGPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetDSVGPUDescriptorHandle(uint32_t index);

public:
	/// <summary>
	/// デスクリプタヒープを生成する
	/// </summary>
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

	Microsoft::WRL::ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

private:
	/// <summary>
	/// 指定番号のCPUデスクリプタハンドルを取得する
	/// </summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr <ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	/// <summary>
	/// 指定番号のGPUデスクリプタハンドルを取得する
	/// </summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	Microsoft::WRL::ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(Microsoft::WRL::ComPtr <ID3D12Device> device, int32_t width, int32_t height);


private:
	// デバイスの初期化
	void InitializeDevice();
	// コマンド関連の初期化
	void InitializeCommand();
	// スワップチェーンの生成
	void CreateSwapChain();
	// 深度バッファの生成
	void CreateDepthBuffer();
	// 各種デスクリプタヒープの生成
	void CreateDescriptorHeap();
	// レンダーターゲットビューの初期化
	void InitializeRenderTargetView();
	// 深度ステンシルビューの初期化
	void InitializeDepthStencilView();
	// フェンスの生成
	void CreateFence();
	// ビューポート矩形の初期化
	void InitializeViewport();
	// シザリング矩形の初期化
	void InitializeScissor();
	// DXCコンパイラの生成
	void CreateDXCCompiler();
#ifndef _Debug
	// ImGuiの初期化
	void InitializeImGui();
#endif // !_Debug

private:
	// WindowsAPI
	WinApp* winApp_ = nullptr;

	Microsoft::WRL::ComPtr <ID3D12Device> device;						// DirectX12デバイス
	Microsoft::WRL::ComPtr <IDXGIFactory7> dxgiFactory;					//DXGIファクトリーの生成
	Microsoft::WRL::ComPtr <ID3D12CommandAllocator> commandAllocator;	// コマンドアロケータ
	Microsoft::WRL::ComPtr <ID3D12GraphicsCommandList> commandList;		// コマンドリスト
	Microsoft::WRL::ComPtr <ID3D12CommandQueue> commandQueue;			// コマンドキュー
	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain;					// スワップチェーン
	// 深度バッファの生成

	std::array< Microsoft::WRL::ComPtr <ID3D12Resource>, 2> swapChainResources_;

	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> rtvDescriptorHeap;	// RTV用のデスクリプタヒープ生成
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> srvDescriptorHeap;	// SRV用のデスクリプタヒープ生成
	Microsoft::WRL::ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap;	// DSV用のデスクリプタヒープ生成
	Microsoft::WRL::ComPtr <ID3D12Fence> fence;
	Microsoft::WRL::ComPtr <IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr <IDxcIncludeHandler> includeHandler;

	UINT64 fenceValue = 0; // フェンス値

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};								// スワップチェーンデスクリプタ
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle;
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
	
	IDxcCompiler3* dxcCompiler;

	static const UINT bufferCount = 2;// バッファ数
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[bufferCount];

private:
	UINT descriptorSizeRTV;  // RTV用のデスクリプタヒープのサイズ
	UINT descriptorSizeSRV;  // SRV用のデスクリプタヒープのサイズ
	UINT descriptorSizeDSV;  // DSV用のデスクリプタヒープのサイズ
};

