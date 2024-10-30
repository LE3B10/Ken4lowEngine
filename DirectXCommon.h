#pragma once
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "DX12Descriptor.h"

#include <chrono>
#include <dxcapi.h>
#include <memory>
#include <thread>

/// ---------- 前方宣言 ---------- ///
class WinApp;

/// -------------------------------------------------------------
///			DirectXCommon - DirectX12の基盤クラス
/// -------------------------------------------------------------
class DirectXCommon
{
	uint32_t kClientWidth;
	uint32_t kClientHeight;

public:
	/// ---------- メンバ関数 ---------- ///

	// シングルトン
	static DirectXCommon* GetInstance();

	// 初期化処理
	void Initialize(WinApp* winApp, uint32_t Width, uint32_t Height);

	// 描画開始・終了処理
	void BeginDraw();
	void EndDraw();

	// 終了処理
	void Finalize();

public:
	/// ---------- ゲッター ---------- ///
	
	ID3D12Device* GetDevice() const;
	ID3D12GraphicsCommandList* GetCommandList() const;
	DX12Descriptor* GetDescriptorHeap() const;
	ID3D12DescriptorHeap* GetSRVDescriptorHeap() const;
	IDxcUtils* GetIDxcUtils() const;
	IDxcCompiler3* GetIDxcCompiler() const;
	IDxcIncludeHandler* GetIncludeHandler() const;
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc();

private:
	/// ---------- メンバ関数 ---------- ///

	// デバッグレイヤーの表示
	void DebugLayer();

	// エラー警告
	void ErrorWarning();

	// コマンド関連の生成
	void CreateCommands();

	// フェンスの生成
	void CreateFenceEvent();

	// DXCコンパイラの生成
	void CreateDXCCompiler();

	// バリアで書き込み可能に変更
	void ChangeBarrier();

	// 画面全体をクリア
	void ClearWindow();

	// FPS固定初期化処理
	void InitializeFixFPS();
	
	// FPS固定更新
	void UpdateFixFPS();

private:
	/// ---------- メンバ変数 ---------- ///
	
	std::unique_ptr<DX12Device> device_;
	std::unique_ptr<DX12SwapChain> swapChain_;
	std::unique_ptr<DX12Descriptor> descriptor;

	Microsoft::WRL::ComPtr <ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	Microsoft::WRL::ComPtr <IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
	Microsoft::WRL::ComPtr <IDxcIncludeHandler> includeHandler;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	Microsoft::WRL::ComPtr <ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue = 0;
	
	D3D12_RESOURCE_BARRIER barrier{};

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex;

private:
	/// ---------- メンバ変数 ---------- ///
	
	// 記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_;

	DirectXCommon() = default;
	~DirectXCommon() = default;

	// コピー禁止
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;
};

