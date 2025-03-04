#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "FPSCounter.h"

#include <dxcapi.h>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class WinApp;
class DX12Descriptor;

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

	// コマンドの実行待ち
	void WaitCommand();

	// 終了処理
	void Finalize();

public:
	/// ---------- ゲッター ---------- ///
	
	ID3D12Device* GetDevice() const;
	ID3D12GraphicsCommandList* GetCommandList() const;
	DX12Descriptor* GetDescriptorHeap() const;
	IDxcUtils* GetIDxcUtils() const;
	IDxcCompiler3* GetIDxcCompiler() const;
	IDxcIncludeHandler* GetIncludeHandler() const;
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc();
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get();}
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
	// FPSの取得
	FPSCounter& GetFPSCounter() { return fpsCounter_; }	

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
	
private: /// ---------- メンバ変数 ---------- ///
	
	FPSCounter fpsCounter_;

	std::unique_ptr<DX12Device> device_;
	std::unique_ptr<DX12SwapChain> swapChain_;
	std::unique_ptr<DX12Descriptor> descriptor;

	ComPtr <ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ComPtr <IDxcUtils> dxcUtils;
	ComPtr<IDxcCompiler3> dxcCompiler;
	ComPtr <IDxcIncludeHandler> includeHandler;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	ComPtr <ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue = 0;
	
	D3D12_RESOURCE_BARRIER barrier{};

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex;

	// FPS計測用
	int frameCount_ = 0; // フレーム数
	std::chrono::steady_clock::time_point fpsReference_; // FPS計測用基準時間
	float currentFPS_ = 0.0f; // 現在のFPS

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

