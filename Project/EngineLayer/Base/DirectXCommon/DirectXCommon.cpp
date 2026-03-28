#define NOMINMAX
#include "DirectXCommon.h"
#include "WinApp.h"
#include "SRVManager.h"
#include "DSVManager.h"

#include <cassert>

namespace Ken4lowEngine
{

#pragma comment(lib,"dxcompiler.lib")

	using namespace Microsoft::WRL;

	namespace
	{
		/// <summary>
		/// シャドウマップの最小解像度。
		/// 設定ミスで 0 や極端に小さい値が入るのを防ぐ。
		/// </summary>
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
	void DirectXCommon::Initialize(WinApp* winApp, uint32_t Width, uint32_t Height)
	{
		// 描画基盤を構成する主要マネージャを生成する
		device_ = std::make_unique<DX12Device>();
		swapChain_ = std::make_unique<DX12SwapChain>();
		dxcCompilerManager_ = std::make_unique<DXCCompilerManager>();
		commandManager_ = std::make_unique<DX12CommandManager>();
		fenceManager_ = std::make_unique<DX12FenceManager>();

		kClientWidth = Width;
		kClientHeight = Height;

		// デバッグ支援を有効化する
		DebugLayer();

		// デバイス本体を生成する
		device_->Initialize();

		// エラー・警告時の停止設定を行う
		ErrorWarning();

		// コマンド関連を初期化する
		commandManager_->Initialize(GetDevice());
		commandManager_->SetFenceManager(fenceManager_.get());

		// スワップチェインを生成する
		swapChain_->Initialize(winApp, device_->GetDXGIFactory(), commandManager_->GetCommandQueue(), Width, Height);

		// Alt+Enter による自動フルスクリーン切替を無効化する
		device_->GetDXGIFactory()->MakeWindowAssociation(winApp->GetHwnd(), DXGI_MWA_NO_ALT_ENTER);

		// フェンスとシェーダコンパイラを初期化する
		fenceManager_->Initialize(GetDevice());
		dxcCompilerManager_->Initialize();

		// RTV / DSV と深度リソースを生成する
		InitializeRTVAndDSV();

		// 通常描画用の Viewport / Scissor を設定する
		viewport = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(kClientWidth), static_cast<float>(kClientHeight), 0.0f, 1.0f);
		scissorRect = D3D12_RECT(0, 0, kClientWidth, kClientHeight);
	}


	/// -------------------------------------------------------------
	///							描画開始処理
	/// -------------------------------------------------------------
	void DirectXCommon::BeginDraw()
	{
		// FPS 計測の開始
		fpsCounter_.StartFrame();

		auto commandList = commandManager_->GetCommandList();

		// 通常描画用の Viewport / Scissor を設定
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);

		// 現在のバックバッファと深度バッファを取得
		backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		ComPtr<ID3D12Resource> backBuffer = GetBackBuffer(backBufferIndex);
		ComPtr<ID3D12Resource> depthBuffer = GetDepthStencilResource();

		// バックバッファを描画可能状態へ遷移
		ResourceTransition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// 深度バッファをシェーダ参照状態へ一度遷移
		ResourceTransition(depthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// 画面全体をクリア
		ClearWindow();
	}


	/// -------------------------------------------------------------
	///							描画終了処理
	/// -------------------------------------------------------------
	void DirectXCommon::EndDraw()
	{
		// 現在のバックバッファを取得
		backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
		ComPtr<ID3D12Resource> backBuffer = GetBackBuffer(backBufferIndex);
		backBuffer->SetName(L"BackBuffer");

		// 表示可能状態へ戻す
		ResourceTransition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		// コマンド実行と GPU 完了待ち
		commandManager_->ExecuteAndWait();
		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();

		// 画面を表示
		swapChain_->GetSwapChain()->Present(1, 0);

		// FPS 計測終了
		fpsCounter_.EndFrame();
	}

	void DirectXCommon::BeginShadowMapPass()
	{
		auto commandList = commandManager_->GetCommandList();

		// シャドウマップ専用の Viewport / Scissor を設定する
		commandList->RSSetViewports(1, &shadowMapViewport);
		commandList->RSSetScissorRects(1, &shadowMapScissorRect);

		// シャドウマップを深度書き込み状態へ遷移する
		if (shadowMapState_ != D3D12_RESOURCE_STATE_DEPTH_WRITE)
		{
			ResourceTransition(shadowMapResource_.Get(), shadowMapState_, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			shadowMapState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE shadowDsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(shadowMapDsvIndex_);

		// シャドウパスでは色情報を書かないため RTV は使わず DSV のみ設定する
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsvHandle);

		// 前フレームの深度を消して新しいシャドウマップを書けるようにする
		commandList->ClearDepthStencilView(
			shadowDsvHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,
			0,
			0,
			nullptr
		);
	}

	void DirectXCommon::EndShadowMapPass()
	{
		// 本描画側でサンプリングできるよう SRV 用ステートへ戻す
		if (shadowMapState_ != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			ResourceTransition(shadowMapResource_.Get(), shadowMapState_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			shadowMapState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}

		// 通常描画用の Viewport / Scissor へ戻す
		auto commandList = commandManager_->GetCommandList();
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	void DirectXCommon::CreateShadowMapSRV()
	{
		if (!shadowMapResource_) return;

		// SRV 未作成なら最初にディスクリプタを確保する
		if (!hasCreatedShadowMapSRV_)
		{
			shadowMapSrvIndex_ = SRVManager::GetInstance()->Allocate();
			hasCreatedShadowMapSRV_ = true;
		}

		// シャドウマップをシェーダから参照するための SRV を作成する
		SRVManager::GetInstance()->CreateSRVForShadowMap(shadowMapSrvIndex_, shadowMapResource_.Get());
	}


	/// -------------------------------------------------------------
	///							終了処理
	/// -------------------------------------------------------------
	void DirectXCommon::Finalize()
	{
		if (!commandManager_ || !fenceManager_) { return; }

		// GPU の処理完了を待ってから破棄する
		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();

		// 深度リソースを先に解放する
		depthStencilResource.Reset();

		// 各マネージャの終了処理
		fenceManager_->Finalize();
		commandManager_->Finalize();
		dxcCompilerManager_->Finalize();
		swapChain_->Finalize();

		// シャドウマップも解放する
		shadowMapResource_.Reset();

		RTVManager::GetInstance()->Finalize();
		DSVManager::GetInstance()->Finalize();

		// 所有している unique_ptr を解放
		fenceManager_.reset();
		commandManager_.reset();
		dxcCompilerManager_.reset();
		swapChain_.reset();

		// デバイスを最後に解放
		device_->Finalize();
		device_.reset();
	}


	/// -------------------------------------------------------------
	///						画面サイズ変更
	/// -------------------------------------------------------------
	void DirectXCommon::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		// リサイズ中に GPU が古いバックバッファを参照しないよう待機する
		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();

		kClientWidth = width;
		kClientHeight = height;

		// 深度バッファを破棄して作り直す
		depthStencilResource.Reset();

		// スワップチェインを新サイズへ変更
		swapChain_->Resize(width, height);

		// バックバッファ RTV を作り直す
		for (uint32_t i = 0; i < 2; i++)
		{
			RTVManager::GetInstance()->CreateRTVForTexture2D(i, swapChain_->GetSwapChainResources(i));
		}

		// メイン深度バッファを再生成する
		D3D12_CLEAR_VALUE clearValue{};
		depthStencilResource = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			kClientWidth, kClientHeight, DXGI_FORMAT_D24_UNORM_S8_UINT, clearValue
		);
		DSVManager::GetInstance()->CreateDSVForDepthBuffer(dsvIndex_, depthStencilResource.Get());

		// シャドウマップも現在設定の解像度で再生成する
		shadowMapResource_.Reset();
		CreateShadowMapResources(false);

		// 通常描画用の Viewport / Scissor を更新する
		viewport = D3D12_VIEWPORT(0.0f, 0.0f, static_cast<float>(kClientWidth), static_cast<float>(kClientHeight), 0.0f, 1.0f);
		scissorRect = D3D12_RECT(0, 0, kClientWidth, kClientHeight);
	}

	void DirectXCommon::SetShadowMapSize(uint32_t width, uint32_t height)
	{
		// 片方だけ変更したい場合でも扱いやすいよう、現在設定をコピーして更新する
		ShadowMapSettings settings = shadowMapSettings_;
		settings.width = std::max(width, kMinShadowMapSize);
		settings.height = std::max(height, kMinShadowMapSize);

		SetShadowMapSettings(settings);
	}

	void DirectXCommon::SetShadowMapSettings(const ShadowMapSettings& settings)
	{
		const uint32_t newWidth = std::max(settings.width, kMinShadowMapSize);
		const uint32_t newHeight = std::max(settings.height, kMinShadowMapSize);

		// 値が変わっていなければ何もしない
		if (shadowMapSettings_.width == newWidth && shadowMapSettings_.height == newHeight)
		{
			return;
		}

		// 新しい設定を保存する
		shadowMapSettings_.width = newWidth;
		shadowMapSettings_.height = newHeight;

		// 初期化前なら設定だけ保持して終了する
		if (!device_ || !commandManager_ || !fenceManager_)
		{
			return;
		}

		// 使用中の可能性があるため GPU 完了待ち後にシャドウマップを作り直す
		fenceManager_->Signal(commandManager_->GetCommandQueue());
		fenceManager_->Wait();

		shadowMapResource_.Reset();
		CreateShadowMapResources(false);
	}


	/// -------------------------------------------------------------
	///						バッファを取得
	/// -------------------------------------------------------------
	ComPtr<ID3D12Resource> DirectXCommon::GetBackBuffer(uint32_t index)
	{
		ComPtr<ID3D12Resource> backBuffer;
		HRESULT hr = S_FALSE;
		hr = swapChain_->GetSwapChain()->GetBuffer(index, IID_PPV_ARGS(&backBuffer));
		assert(SUCCEEDED(hr));
		return backBuffer;
	}


	D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetShadowMapSrvHandleGPU() const
	{
		return SRVManager::GetInstance()->GetGPUDescriptorHandle(shadowMapSrvIndex_);
	}

#pragma region デバッグレイヤーと警告時に停止処理
	/// -------------------------------------------------------------
	///					デバッグレイヤーの表示
	/// -------------------------------------------------------------
	void DirectXCommon::DebugLayer()
	{
#ifdef _DEBUG
		ComPtr <ID3D12Debug1> debugController = nullptr;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			// CPU / GPU 側のデバッグ支援を有効化する
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
			// 深刻な問題やエラー・警告発生時に停止する
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

			// Windows 環境依存で出る既知の不要メッセージは抑制する
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


	/// -------------------------------------------------------------
	///					画面全体のクリア処理
	/// -------------------------------------------------------------
	void DirectXCommon::ClearWindow()
	{
		auto commandList = commandManager_->GetCommandList();

		backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();

		// 現在のバックバッファ RTV と深度 DSV を取得する
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(backBufferIndex);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(dsvIndex_);

		// 深度バッファを書き込み状態へ戻す
		ComPtr<ID3D12Resource> depthBuffer = GetDepthStencilResource();
		ResourceTransition(depthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		// 描画先を現在のバックバッファと深度バッファへ設定する
		commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

		// カラーと深度をクリアする
		float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}


	/// -------------------------------------------------------------
	///					　RTVとDSVの初期化処理
	/// -------------------------------------------------------------
	void DirectXCommon::InitializeRTVAndDSV()
	{
		// DSV マネージャを初期化する
		DSVManager::GetInstance()->Initialize(this);

		// メイン深度バッファを生成する
		D3D12_CLEAR_VALUE clearValue{};
		depthStencilResource = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			kClientWidth, kClientHeight, DXGI_FORMAT_D24_UNORM_S8_UINT, clearValue
		);
		depthStencilResource->SetName(L"DepthStencilBuffer");

		// メイン深度バッファ用 DSV を作成する
		dsvIndex_ = DSVManager::GetInstance()->Allocate();
		DSVManager::GetInstance()->CreateDSVForDepthBuffer(dsvIndex_, depthStencilResource.Get());

		// 現在の設定値でシャドウマップ関連リソースを生成する
		CreateShadowMapResources(true);

		// RTV マネージャを初期化する
		RTVManager::GetInstance()->Initialize(this);

		// スワップチェインの各バックバッファに RTV を作成する
		for (uint32_t i = 0; i < 2; i++)
		{
			uint32_t rtvIndex = RTVManager::GetInstance()->Allocate();
			RTVManager::GetInstance()->CreateRTVForTexture2D(rtvIndex, swapChain_->GetSwapChainResources(i));
			swapChain_->GetSwapChainResources(i)->SetName((L"BackBuffer_" + std::to_wstring(i)).c_str());
		}
	}

	void DirectXCommon::CreateShadowMapResources(bool allocateDescriptor)
	{
		// 既存のシャドウマップを破棄して作り直す
		shadowMapResource_.Reset();

		// 現在設定されている解像度でシャドウマップ深度リソースを生成する
		shadowMapResource_ = DSVManager::GetInstance()->CreateShadowMapResource(
			shadowMapSettings_.width,
			shadowMapSettings_.height
		);
		shadowMapResource_->SetName(L"ShadowMapResource");

		// 初回生成時のみ DSV ディスクリプタを確保する
		if (allocateDescriptor && !hasCreatedShadowMapDSV_)
		{
			shadowMapDsvIndex_ = DSVManager::GetInstance()->Allocate();
			hasCreatedShadowMapDSV_ = true;
		}

		// シャドウマップ用 DSV を更新する
		DSVManager::GetInstance()->CreateDSVForShadowMap(shadowMapDsvIndex_, shadowMapResource_.Get());

		// 解像度に合わせてシャドウパス専用の Viewport / Scissor を作成する
		shadowMapViewport = D3D12_VIEWPORT(
			0.0f,
			0.0f,
			static_cast<float>(shadowMapSettings_.width),
			static_cast<float>(shadowMapSettings_.height),
			0.0f,
			1.0f
		);

		shadowMapScissorRect = D3D12_RECT(
			0,
			0,
			static_cast<LONG>(shadowMapSettings_.width),
			static_cast<LONG>(shadowMapSettings_.height)
		);

		// 生成直後は深度書き込み状態から開始する
		shadowMapState_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		// すでに SRV を確保済みなら、新しいリソースに張り替える
		if (hasCreatedShadowMapSRV_)
		{
			SRVManager::GetInstance()->CreateSRVForShadowMap(shadowMapSrvIndex_, shadowMapResource_.Get());
		}
	}


} // namespace Ken4lowEngine