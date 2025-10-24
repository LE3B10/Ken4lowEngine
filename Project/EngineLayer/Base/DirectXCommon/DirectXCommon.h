#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "FPSCounter.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "DXCCompilerManager.h"
#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

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

	// 終了処理
	void Finalize();

	// リソース遷移の管理
	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
	{
		commandManager_->ResourceTransition(resource, stateBefore, stateAfter);
	}

public: /// ---------- ゲッター ---------- ///

	ID3D12Device* GetDevice() const { return device_->GetDevice(); }
	DX12SwapChain* GetSwapChain() { return swapChain_.get(); }
	DXCCompilerManager* GetDXCCompilerManager() { return dxcCompilerManager_.get(); }
	DX12CommandManager* GetCommandManager() { return commandManager_.get(); }
	DX12FenceManager* GetFenceManager() { return fenceManager_.get(); }

	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }
	// FPSの取得
	FPSCounter& GetFPSCounter() { return fpsCounter_; }

	// バックバッファを取得
	ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

	// RTVのバックバッファを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) { return RTVManager::GetInstance()->GetCPUDescriptorHandle(index); }

	// 深度ステンシルリソースを取得
	ComPtr<ID3D12Resource> GetDepthStencilResource() const { return depthStencilResource.Get(); }

private: /// ---------- メンバ関数 ---------- ///

	// デバッグレイヤーの表示
	void DebugLayer();

	// エラー警告
	void ErrorWarning();

	// 画面全体をクリア
	void ClearWindow();

	// RTVとDSVの初期化関数を追加
	void InitializeRTVAndDSV();

private: /// ---------- メンバ変数 ---------- ///

	FPSCounter fpsCounter_;

	std::unique_ptr<DX12Device> device_;
	std::unique_ptr<DX12SwapChain> swapChain_;
	std::unique_ptr<DXCCompilerManager> dxcCompilerManager_;
	std::unique_ptr<DX12CommandManager> commandManager_;
	std::unique_ptr<DX12FenceManager> fenceManager_;

	D3D12_RESOURCE_BARRIER barrier{};

	// 描画開始・終了処理に使う
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	UINT backBufferIndex = 0;
	uint32_t dsvIndex_ = 0; // 🔹 DSVのインデックス

	ComPtr<ID3D12Resource> depthStencilResource; // 🔹 深度バッファ

private: /// ---------- コピー禁止 ---------- ///

	DirectXCommon() = default;
	~DirectXCommon() = default;
	DirectXCommon(const DirectXCommon&) = delete;
	const DirectXCommon& operator=(const DirectXCommon&) = delete;
};
