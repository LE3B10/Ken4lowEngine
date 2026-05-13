#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "DXCCompilerManager.h"
#include "DX12CommandManager.h"
#include "DX12FenceManager.h"
#include "MainRenderTarget.h"
#include "ShadowMapRenderTarget.h"
#include "PipelineFactory.h"

#include <dxcapi.h>
#include <memory>
#include <algorithm>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class WinApp;

	/// -------------------------------------------------------------
	///			DirectX12 基盤の司令塔クラス
	/// -------------------------------------------------------------
	/// Device / SwapChain / Command / Fence / RenderTarget の
	/// 初期化順とフレーム進行を統括する。
	class DirectXCommon
	{
	public:
		/// ---------------------------------------------------------
		///			シングルトン取得
		/// ---------------------------------------------------------
		static DirectXCommon* GetInstance();

		/// ---------------------------------------------------------
		///				初期化 / 終了
		/// ---------------------------------------------------------
		void Initialize(WinApp* winApp, uint32_t width, uint32_t height);
		void Finalize();

		/// ---------------------------------------------------------
		///			フレーム描画開始 / 終了
		/// ---------------------------------------------------------
		void BeginDraw();

		/// ---------------------------------------------------------
		///		ImGui描画前のバックバッファRTV設定
		/// ---------------------------------------------------------
		void PrepareBackBufferForImGui();

		/// ---------------------------------------------------------
		///		Release/Game描画前のバックバッファRTV設定
		/// ---------------------------------------------------------
		void PrepareBackBufferForGame();

		/// ---------------------------------------------------------
		///		Release/GameのHUD描画前バックバッファ再バインド
		/// ---------------------------------------------------------
		void RebindBackBufferForGameOverlay();

		void EndDraw();

		/// ---------------------------------------------------------
		///			シャドウパス開始 / 終了
		/// ---------------------------------------------------------
		void BeginShadowMapPass();
		void EndShadowMapPass();

		/// ---------------------------------------------------------
		///				リソースステート遷移
		/// ---------------------------------------------------------
		void ResourceTransition(
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES stateBefore,
			D3D12_RESOURCE_STATES stateAfter)
		{
			commandManager_->ResourceTransition(resource, stateBefore, stateAfter);
		}

		/// ---------------------------------------------------------
		///				リサイズ
		/// ---------------------------------------------------------
		void Resize(uint32_t width, uint32_t height);

		/// ---------------------------------------------------------
		///		シャドウマップサイズ変更
		/// ---------------------------------------------------------
		void SetShadowMapSize(uint32_t width, uint32_t height);

	public: /// ---------- Getter ---------- ///

		ID3D12Device* GetDevice() const { return device_->GetDevice(); }
		DX12SwapChain* GetSwapChain() { return swapChain_.get(); }
		DXCCompilerManager* GetDXCCompilerManager() { return dxcCompilerManager_.get(); }
		DX12CommandManager* GetCommandManager() { return commandManager_.get(); }
		DX12FenceManager* GetFenceManager() { return fenceManager_.get(); }
		DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }

		/// 指定インデックスのバックバッファを取得
		ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

		/// バックバッファ数を取得
		uint32_t GetBackBufferCount() const
		{
			return swapChain_->GetSwapChainDesc().BufferCount;
		}

		/// 指定インデックスのバックバッファ RTV を取得
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) const
		{
			return mainRenderTarget_->GetRtvHandleCPU(index);
		}

		uint32_t GetClientWidth() const { return clientWidth_; }
		uint32_t GetClientHeight() const { return clientHeight_; }

		MainRenderTarget* GetMainRenderTarget() const { return mainRenderTarget_.get(); }
		ShadowMapRenderTarget* GetShadowMapRenderTarget() const { return shadowMapRenderTarget_.get(); }

		uint32_t GetShadowMapSrvIndex() const
		{
			return shadowMapRenderTarget_ ? shadowMapRenderTarget_->GetSrvIndex() : UINT32_MAX;
		}

		D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvHandleGPU() const
		{
			return shadowMapRenderTarget_->GetSrvHandleGPU();
		}

		PipelineFactory& GetPipelineFactory() { return pipelineFactory_; }
		const PipelineFactory& GetPipelineFactory() const { return pipelineFactory_; }

	private: /// ---------- 初期化分割 ---------- ///

		void InitializeCoreObjects();
		void InitializeCoreSystems(WinApp* winApp, uint32_t width, uint32_t height);
		void InitializeManagers();
		void InitializeRenderTargets();

	private: /// ---------- 補助関数 ---------- ///

		void WaitForGpuIdle();

		// デバッグレイヤー有効化
		void DebugLayer();

		// エラー・警告発生時の停止設定
		void ErrorWarning();

	private: /// ---------- メンバ変数 ---------- ///

		// クライアント領域サイズ
		uint32_t clientWidth_ = 0;
		uint32_t clientHeight_ = 0;

		std::unique_ptr<DX12Device> device_;
		std::unique_ptr<DX12SwapChain> swapChain_;
		std::unique_ptr<DXCCompilerManager> dxcCompilerManager_;
		std::unique_ptr<DX12CommandManager> commandManager_;
		std::unique_ptr<DX12FenceManager> fenceManager_;

		PipelineFactory pipelineFactory_;

		// 分離済み描画先
		std::unique_ptr<MainRenderTarget> mainRenderTarget_;
		std::unique_ptr<ShadowMapRenderTarget> shadowMapRenderTarget_;

		UINT backBufferIndex_ = 0;

	private: /// ---------- コピー禁止 ---------- ///

		DirectXCommon() = default;
		~DirectXCommon() = default;
		DirectXCommon(const DirectXCommon&) = delete;
		const DirectXCommon& operator=(const DirectXCommon&) = delete;
	};

} // namespace Ken4lowEngine