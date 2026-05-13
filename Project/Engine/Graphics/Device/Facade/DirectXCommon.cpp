#define NOMINMAX
#include "DirectXCommon.h"
#include "WinApp.h"
#include "RTVManager.h"
#include "DSVManager.h"
#include "SRVManager.h"

#include <cassert>

namespace Ken4lowEngine
{

#pragma comment(lib,"dxcompiler.lib")

	using namespace Microsoft::WRL;

	namespace
	{
		/// 設定ミス防止用の最小サイズ
		constexpr uint32_t kMinShadowMapSize = 1;
	}

	/// -------------------------------------------------------------
	///					シングルトンインスタンス
	/// -------------------------------------------------------------
	DirectXCommon* DirectXCommon::GetInstance()
	{
		static DirectXCommon instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void DirectXCommon::Initialize(WinApp* winApp, uint32_t width, uint32_t height)
	{
		clientWidth_ = width;
		clientHeight_ = height;

		InitializeCoreObjects();
		InitializeCoreSystems(winApp, width, height);
		InitializeManagers();
		InitializeRenderTargets();
	}

	/// -------------------------------------------------------------
	///				DX12基盤オブジェクト生成
	/// -------------------------------------------------------------
	void DirectXCommon::InitializeCoreObjects()
	{
		device_ = std::make_unique<DX12Device>();
		swapChain_ = std::make_unique<DX12SwapChain>();
		dxcCompilerManager_ = std::make_unique<DXCCompilerManager>();
		commandManager_ = std::make_unique<DX12CommandManager>();
		fenceManager_ = std::make_unique<DX12FenceManager>();
	}

	/// -------------------------------------------------------------
	///				Device / SwapChain / Command 初期化
	/// -------------------------------------------------------------
	void DirectXCommon::InitializeCoreSystems(WinApp* winApp, uint32_t width, uint32_t height)
	{
		DebugLayer();

		// Device
		device_->Initialize();
		ErrorWarning();

		// Command / Fence
		commandManager_->Initialize(GetDevice());
		commandManager_->SetFenceManager(fenceManager_.get());
		fenceManager_->Initialize(GetDevice());

		// SwapChain
		swapChain_->Initialize(
			winApp,
			device_->GetDXGIFactory(),
			commandManager_->GetCommandQueue(),
			width,
			height
		);

		// Alt+Enter の自動フルスクリーン切替を無効化
		device_->GetDXGIFactory()->MakeWindowAssociation(winApp->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);

		// Compiler
		dxcCompilerManager_->Initialize();

		// PipelineFactory
		pipelineFactory_.Initialize(GetDevice());
	}

	/// -------------------------------------------------------------
	///					各Manager初期化
	/// -------------------------------------------------------------
	void DirectXCommon::InitializeManagers()
	{
		DSVManager::GetInstance()->Initialize(this);
		RTVManager::GetInstance()->Initialize(this);
		SRVManager::GetInstance()->Initialize(this);
	}

	/// -------------------------------------------------------------
	///				描画先オブジェクト初期化
	/// -------------------------------------------------------------
	void DirectXCommon::InitializeRenderTargets()
	{
		// メイン描画先
		MainRenderTargetSettings mainSettings{};
		mainRenderTarget_ = std::make_unique<MainRenderTarget>();
		mainRenderTarget_->Initialize(this, mainSettings);

		// シャドウ描画先
		ShadowMapSettings shadowSettings{};
		shadowSettings.width = 2048;
		shadowSettings.height = 2048;

		shadowMapRenderTarget_ = std::make_unique<ShadowMapRenderTarget>();
		shadowMapRenderTarget_->Initialize(this, shadowSettings);
	}

	/// -------------------------------------------------------------
	///							描画開始処理
	/// -------------------------------------------------------------
	void DirectXCommon::BeginDraw()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_)
		{
			return;
		}

		// DebugはGameViewportRenderTarget経由、Releaseは後段のPrepareBackBufferForGameでBackBufferへ直接バインドする。
		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
	}

	/// -------------------------------------------------------------
	///			ImGui描画前のバックバッファRTV設定
	/// -------------------------------------------------------------
	void DirectXCommon::PrepareBackBufferForImGui()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_)
		{
			return;
		}

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);

		// ImGui_ImplDX12_RenderDrawData() が直前にバインド済みの
		// SceneRenderTarget_GameViewportRenderTargetへ描いてしまわないよう、
		// GameRenderTargetはSRVのまま維持し、BackBufferだけをRTVへ戻してOMへ設定する。
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(
			backBuffer.Get(),
			beforeState,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// BackBufferにはSceneを描かず、ImGui描画直前にだけRTV/Viewport/Scissorを設定してクリアする。
		mainRenderTarget_->Begin(commandManager_->GetCommandList(), backBufferIndex_);
	}


	/// -------------------------------------------------------------
	///			Release/Game描画前のバックバッファRTV設定
	/// -------------------------------------------------------------
	void DirectXCommon::PrepareBackBufferForGame()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_)
		{
			return;
		}

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);

		// Release/GameではImGuiを経由しないため、Scene描画前にBackBufferを直接RTVへ戻す。
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(
			backBuffer.Get(),
			beforeState,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);

		mainRenderTarget_->Begin(commandManager_->GetCommandList(), backBufferIndex_);
	}


	/// -------------------------------------------------------------
	///		Release/GameのHUD描画前バックバッファ再バインド
	/// -------------------------------------------------------------
	void DirectXCommon::RebindBackBufferForGameOverlay()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_)
		{
			return;
		}

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);

		// GPU ParticleがGameViewportRenderTargetを再バインドしても、HUD/UIはReleaseのBackBufferへ重ねる。
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(
			backBuffer.Get(),
			beforeState,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_RENDER_TARGET);

		mainRenderTarget_->Bind(commandManager_->GetCommandList(), backBufferIndex_);
	}


	/// -------------------------------------------------------------
	///							描画終了処理
	/// -------------------------------------------------------------
	void DirectXCommon::EndDraw()
	{
		if (!mainRenderTarget_ || !commandManager_ || !swapChain_ || !fenceManager_)
		{
			return;
		}

		backBufferIndex_ = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		auto backBuffer = GetBackBuffer(backBufferIndex_);

		mainRenderTarget_->End(commandManager_->GetCommandList());

		// BackBufferの記録状態を元にPresentへ戻し、メインRTの状態管理を一箇所に寄せる
		const D3D12_RESOURCE_STATES beforeState = swapChain_->GetBackBufferState(backBufferIndex_);
		ResourceTransition(
			backBuffer.Get(),
			beforeState,
			D3D12_RESOURCE_STATE_PRESENT
		);
		swapChain_->SetBackBufferState(backBufferIndex_, D3D12_RESOURCE_STATE_PRESENT);

		// コマンド実行と GPU 完了待ち
		commandManager_->ExecuteAndWait();
		WaitForGpuIdle();

		// 画面表示
		swapChain_->GetSwapChain()->Present(1, 0);
	}

	/// -------------------------------------------------------------
	///					シャドウパス開始
	/// -------------------------------------------------------------
	void DirectXCommon::BeginShadowMapPass()
	{
		if (!shadowMapRenderTarget_ || !commandManager_)
		{
			return;
		}

		shadowMapRenderTarget_->Begin(commandManager_->GetCommandList());
	}

	/// -------------------------------------------------------------
	///					シャドウパス終了
	/// -------------------------------------------------------------
	void DirectXCommon::EndShadowMapPass()
	{
		if (!shadowMapRenderTarget_ || !commandManager_)
		{
			return;
		}

		shadowMapRenderTarget_->End(commandManager_->GetCommandList());
	}

	/// -------------------------------------------------------------
	///							終了処理
	/// -------------------------------------------------------------
	void DirectXCommon::Finalize()
	{
		if (!commandManager_ || !fenceManager_)
		{
			return;
		}

		WaitForGpuIdle();

		// 分離した描画先を先に閉じる
		if (mainRenderTarget_)
		{
			mainRenderTarget_->Finalize();
			mainRenderTarget_.reset();
		}

		if (shadowMapRenderTarget_)
		{
			shadowMapRenderTarget_->Finalize();
			shadowMapRenderTarget_.reset();
		}

		pipelineFactory_.Finalize();

		// 各マネージャ終了
		fenceManager_->Finalize();
		commandManager_->Finalize();
		dxcCompilerManager_->Finalize();
		swapChain_->Finalize();

		RTVManager::GetInstance()->Finalize();
		DSVManager::GetInstance()->Finalize();
		SRVManager::GetInstance()->Finalize();

		// unique_ptr 解放
		fenceManager_.reset();
		commandManager_.reset();
		dxcCompilerManager_.reset();
		swapChain_.reset();

		// Device を最後に解放
		device_->Finalize();
		device_.reset();
	}

	/// -------------------------------------------------------------
	///						画面サイズ変更
	/// -------------------------------------------------------------
	void DirectXCommon::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		WaitForGpuIdle();

		clientWidth_ = width;
		clientHeight_ = height;

		// SwapChain を先にリサイズ
		swapChain_->Resize(width, height);

		// メイン描画先を更新
		if (mainRenderTarget_)
		{
			mainRenderTarget_->Resize(width, height);
		}

		// シャドウは画面サイズと独立なので、現在サイズのまま再生成だけしておく
		if (shadowMapRenderTarget_)
		{
			const uint32_t shadowWidth =
				std::max(shadowMapRenderTarget_->GetWidth(), kMinShadowMapSize);
			const uint32_t shadowHeight =
				std::max(shadowMapRenderTarget_->GetHeight(), kMinShadowMapSize);

			shadowMapRenderTarget_->Resize(shadowWidth, shadowHeight);
		}
	}

	/// -------------------------------------------------------------
	///					シャドウマップサイズ変更
	/// -------------------------------------------------------------
	void DirectXCommon::SetShadowMapSize(uint32_t width, uint32_t height)
	{
		width = std::max(width, kMinShadowMapSize);
		height = std::max(height, kMinShadowMapSize);

		if (!shadowMapRenderTarget_)
		{
			return;
		}

		WaitForGpuIdle();
		shadowMapRenderTarget_->Resize(width, height);
	}

	/// -------------------------------------------------------------
	///					GPU完了待ち
	/// -------------------------------------------------------------
	void DirectXCommon::WaitForGpuIdle()
	{
		if (!fenceManager_ || !commandManager_)
		{
			return;
		}

		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();
	}

	/// -------------------------------------------------------------
	///						バッファ取得
	/// -------------------------------------------------------------
	ComPtr<ID3D12Resource> DirectXCommon::GetBackBuffer(uint32_t index)
	{
		ComPtr<ID3D12Resource> backBuffer;
		HRESULT hr = swapChain_->GetSwapChain()->GetBuffer(index, IID_PPV_ARGS(&backBuffer));
		assert(SUCCEEDED(hr));
		return backBuffer;
	}

#pragma region デバッグレイヤーと警告時に停止処理

	/// -------------------------------------------------------------
	///					デバッグレイヤーの表示
	/// -------------------------------------------------------------
	void DirectXCommon::DebugLayer()
	{
#ifdef _DEBUG
		ComPtr<ID3D12Debug1> debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(FALSE);
		}
#endif
	}

	/// -------------------------------------------------------------
	///					エラー・警告時の処理
	/// -------------------------------------------------------------
	void DirectXCommon::ErrorWarning()
	{
#ifdef _DEBUG
		ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
		if (SUCCEEDED(GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			D3D12_MESSAGE_ID denyIds[] =
			{
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
			};

			D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = _countof(denyIds);
			filter.DenyList.pIDList = denyIds;
			filter.DenyList.NumSeverities = _countof(severities);
			filter.DenyList.pSeverityList = severities;

			infoQueue->PushStorageFilter(&filter);
		}
#endif
	}

#pragma endregion

} // namespace Ken4lowEngine