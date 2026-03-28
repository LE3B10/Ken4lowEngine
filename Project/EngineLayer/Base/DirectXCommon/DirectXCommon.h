#pragma once
#include "DX12Include.h"
#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "FPSCounter.h"
#include "RTVManager.h"
#include "DXCCompilerManager.h"
#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

#include <dxcapi.h>
#include <memory>
#include <Vector4.h>
#include <algorithm>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class WinApp;


	/// -------------------------------------------------------------
	///			DirectXCommon - DirectX12の基盤クラス
	/// -------------------------------------------------------------
	/// デバイス、スワップチェイン、コマンド、フェンス、RTV/DSV など、
	/// 描画基盤全体の初期化と管理を担当する。
	class DirectXCommon
	{
		// クライアント領域サイズ : 幅
		uint32_t kClientWidth = 0;

		// クライアント領域サイズ : 高さ
		uint32_t kClientHeight = 0;

	public: /// ---------- シャドウマップ設定 ---------- ///

		/// <summary>
		/// シャドウマップ生成に使う設定。
		/// 以前は 2048x2048 を固定値で持っていたが、
		/// 品質設定やライト設定から差し替えやすいように構造体へまとめている。
		/// </summary>
		struct ShadowMapSettings
		{
			uint32_t width = 2048;
			uint32_t height = 2048;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// DirectXCommon のシングルトンインスタンスを取得する。
		/// 描画基盤はアプリ全体で 1 つだけ使う想定のため、シングルトンにしている。
		/// </summary>
		static DirectXCommon* GetInstance();

		/// <summary>
		/// DirectX12 の初期化処理を行う。
		/// デバイス・スワップチェイン・コマンド・フェンス・RTV/DSV などを順に準備し、
		/// 最後に通常描画用の Viewport / ScissorRect を設定する。
		/// </summary>
		void Initialize(WinApp* winApp, uint32_t Width, uint32_t Height);

		/// <summary>
		/// 1 フレーム分の描画開始処理。
		/// バックバッファを描画可能状態へ遷移し、画面クリアまでを行う。
		/// </summary>
		void BeginDraw();

		/// <summary>
		/// 1 フレーム分の描画終了処理。
		/// バックバッファを Present 状態へ戻し、コマンド実行と Present を行う。
		/// </summary>
		void EndDraw();

		/// <summary>
		/// シャドウマップ描画パスを開始する。
		/// シャドウマップ用 Viewport / ScissorRect を設定し、
		/// 深度書き込み用の DSV に切り替える。
		/// </summary>
		void BeginShadowMapPass();

		/// <summary>
		/// シャドウマップ描画パスを終了する。
		/// 本描画でサンプリングできるよう、シャドウマップを SRV 用ステートへ戻す。
		/// </summary>
		void EndShadowMapPass();

		/// <summary>
		/// シャドウマップの SRV を作成する。
		/// シャドウマップをピクセルシェーダから参照するために必要。
		/// </summary>
		void CreateShadowMapSRV();

		/// <summary>
		/// DirectXCommon の終了処理を行う。
		/// GPU の完了待ち後、保持している描画関連リソースを順に破棄する。
		/// </summary>
		void Finalize();

		/// <summary>
		/// 指定リソースのステートを変更するヘルパー。
		/// 内部的にはコマンドリストへ ResourceBarrier を積む。
		/// </summary>
		void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
		{
			commandManager_->ResourceTransition(resource, stateBefore, stateAfter);
		}

		/// <summary>
		/// 画面サイズ変更時の再生成処理。
		/// スワップチェインと深度バッファ、必要に応じてシャドウマップも作り直す。
		/// </summary>
		void Resize(uint32_t width, uint32_t height);

		/// <summary>
		/// シャドウマップ解像度を設定する。
		/// 初期化前は設定だけ保持し、初期化後なら必要に応じてシャドウマップを作り直す。
		/// </summary>
		void SetShadowMapSize(uint32_t width, uint32_t height);

		/// <summary>
		/// シャドウマップ設定をまとめて変更する。
		/// 将来的に品質設定クラスなどから一括適用しやすくするための入口。
		/// </summary>
		void SetShadowMapSettings(const ShadowMapSettings& settings);

	public: /// ---------- ゲッター ---------- ///

		ID3D12Device* GetDevice() const { return device_->GetDevice(); }
		DX12SwapChain* GetSwapChain() { return swapChain_.get(); }
		DXCCompilerManager* GetDXCCompilerManager() { return dxcCompilerManager_.get(); }
		DX12CommandManager* GetCommandManager() { return commandManager_.get(); }
		DX12FenceManager* GetFenceManager() { return fenceManager_.get(); }
		DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() const { return swapChain_->GetSwapChainDesc(); }
		FPSCounter& GetFPSCounter() { return fpsCounter_; }

		/// <summary>
		/// 指定インデックスのバックバッファを取得する。
		/// </summary>
		ComPtr<ID3D12Resource> GetBackBuffer(uint32_t index);

		/// <summary>
		/// 指定インデックスのバックバッファ RTV を取得する。
		/// </summary>
		D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV(uint32_t index) { return RTVManager::GetInstance()->GetCPUDescriptorHandle(index); }

		/// <summary>
		/// 深度ステンシルバッファを取得する。
		/// </summary>
		ComPtr<ID3D12Resource> GetDepthStencilResource() const { return depthStencilResource.Get(); }

		uint32_t GetClientWidth()  const { return kClientWidth; }
		uint32_t GetClientHeight() const { return kClientHeight; }

		uint32_t GetShadowMapSrvIndex() const { return shadowMapSrvIndex_; }

		/// <summary>
		/// シャドウマップ SRV の GPU ハンドルを取得する。
		/// </summary>
		D3D12_GPU_DESCRIPTOR_HANDLE GetShadowMapSrvHandleGPU() const;

		/// <summary>
		/// 現在のシャドウマップ設定を取得する。
		/// </summary>
		const ShadowMapSettings& GetShadowMapSettings() const { return shadowMapSettings_; }

	private: /// ---------- メンバ関数 ---------- ///

		// デバッグレイヤーを有効化する
		void DebugLayer();

		// エラー・警告発生時の停止設定を行う
		void ErrorWarning();

		// 画面全体のカラーと深度をクリアする
		void ClearWindow();

		// RTV と DSV を初期化する
		void InitializeRTVAndDSV();

		// シャドウマップ用深度リソースと Viewport / Scissor を作成する
		void CreateShadowMapResources(bool allocateDescriptor);

	private: /// ---------- メンバ変数 ---------- ///

		FPSCounter fpsCounter_;

		std::unique_ptr<DX12Device> device_;
		std::unique_ptr<DX12SwapChain> swapChain_;
		std::unique_ptr<DXCCompilerManager> dxcCompilerManager_;
		std::unique_ptr<DX12CommandManager> commandManager_;
		std::unique_ptr<DX12FenceManager> fenceManager_;

		D3D12_RESOURCE_BARRIER barrier{};

		// 通常描画用の Viewport / Scissor
		D3D12_VIEWPORT viewport{};
		D3D12_RECT scissorRect{};

		UINT backBufferIndex = 0;
		uint32_t dsvIndex_ = 0;

		// メイン描画用深度バッファ
		ComPtr<ID3D12Resource> depthStencilResource;

	private: /// ---------- シャドウマップ用変数 ---------- ///

		// シャドウマップ解像度設定
		ShadowMapSettings shadowMapSettings_{};

		uint32_t shadowMapDsvIndex_ = 0;
		uint32_t shadowMapSrvIndex_ = 0;

		bool hasCreatedShadowMapDSV_ = false;
		bool hasCreatedShadowMapSRV_ = false;

		// シャドウマップ専用の Viewport / Scissor
		D3D12_VIEWPORT shadowMapViewport{};
		D3D12_RECT shadowMapScissorRect{};

		// シャドウマップ深度バッファ
		ComPtr<ID3D12Resource> shadowMapResource_;

		// シャドウマップの現在ステート
		D3D12_RESOURCE_STATES shadowMapState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	private: /// ---------- コピー禁止 ---------- ///

		DirectXCommon() = default;
		~DirectXCommon() = default;
		DirectXCommon(const DirectXCommon&) = delete;
		const DirectXCommon& operator=(const DirectXCommon&) = delete;
	};

} // namespace Ken4lowEngine