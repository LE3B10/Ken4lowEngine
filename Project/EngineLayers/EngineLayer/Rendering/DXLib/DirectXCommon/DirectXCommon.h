#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "FPSCounter.h"
#include "RTVManager.h"
#include "DSVManager.h"

#include <dxcapi.h>
#include <memory>
#include <Vector4.h>

/// ---------- 前方宣言 ---------- ///
class WinApp;


/// -------------------------------------------------------------
///			DirectXCommon - DirectX12の基盤クラス
/// -------------------------------------------------------------
class DirectXCommon
{
	uint32_t kClientWidth;
	uint32_t kClientHeight;

public: /// ---------- メンバ関数 ---------- ///

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

	// リソース遷移の管理
	void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

public: /// ---------- ゲッター ---------- ///

	ID3D12Device* GetDevice() const { return device_->GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	DX12SwapChain* GetSwapChain() { return swapChain_.get(); }
	IDxcUtils* GetIDxcUtils() const { return dxcUtils.Get(); }
	IDxcCompiler3* GetIDxcCompiler() const { return dxcCompiler.Get(); }
	IDxcIncludeHandler* GetIncludeHandler() const { return includeHandler.Get(); }
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
	// FPSの取得
	FPSCounter& GetFPSCounter() { return fpsCounter_; }

	ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) { return RTVManager::GetInstance()->GetCPUDescriptorHandle(index); }

private: /// ---------- メンバ関数 ---------- ///

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

	// 🔹 RTVとDSVの初期化関数を追加
	void InitializeRTVAndDSV();

private: /// ---------- メンバ変数 ---------- ///

	FPSCounter fpsCounter_;

	std::unique_ptr<DX12Device> device_;
	std::unique_ptr<DX12SwapChain> swapChain_;

	ComPtr <ID3D12CommandQueue> commandQueue;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> commandList_;

	ComPtr <IDxcUtils> dxcUtils;
	ComPtr<IDxcCompiler3> dxcCompiler;
	ComPtr <IDxcIncludeHandler> includeHandler;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	ComPtr <ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue = 0;

	D3D12_RESOURCE_BARRIER barrier{};

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex = 0;

	ComPtr<ID3D12Resource> depthStencilResource; // 🔹 深度バッファ

private: /// ---------- コピー禁止 ---------- ///

	DirectXCommon() = default;
	~DirectXCommon() = default;
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;
};
