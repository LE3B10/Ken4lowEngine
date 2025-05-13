#include "DirectXCommon.h"
#include "WinApp.h"
#include "PostEffectManager.h"

#include <cassert>
#include <Wireframe.h>
#include <SceneManager.h>
#include <ParticleManager.h>

#include "ImGuiManager.h"


#pragma comment(lib,"dxcompiler.lib")

using namespace Microsoft::WRL;


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
	device_ = std::make_unique<DX12Device>();
	swapChain_ = std::make_unique<DX12SwapChain>();
	dxcCompilerManager_ = std::make_unique<DXCCompilerManager>();
	commandManager_ = std::make_unique<DX12CommandManager>();
	fenceManager_ = std::make_unique<DX12FenceManager>();

	kClientWidth = Width;
	kClientHeight = Height;

	// デバッグレイヤーをオンに
	DebugLayer();

	// デバイスの初期化
	device_->Initialize();

	// エラー、警告
	ErrorWarning();

	// コマンド生成
	commandManager_->Initialize(device_->GetDevice());
	commandManager_->SetFenceManager(fenceManager_.get());

	// スワップチェインの生成
	swapChain_->Initialize(winApp, device_->GetDXGIFactory(), commandManager_->GetCommandQueue(), Width, Height);

	// フェンスとイベントの生成
	fenceManager_->Initialize(device_->GetDevice());

	// DXCコンパイラの生成
	dxcCompilerManager_->Initialize();

	// RTV & DSVの初期化処理
	InitializeRTVAndDSV();

	// ビューポート矩形の設定
	viewport = D3D12_VIEWPORT(0.0f, 0.0f, (float)kClientWidth, (float)kClientHeight, 0.0f, 1.0f);

	// シザリング矩形の設定
	scissorRect = D3D12_RECT(0, 0, kClientWidth, kClientHeight);
}


/// -------------------------------------------------------------
///							描画開始処理
/// -------------------------------------------------------------
void DirectXCommon::BeginDraw()
{
	// FPSカウンターの開始
	fpsCounter_.StartFrame();

	auto commandList = commandManager_->GetCommandList();

	commandList->RSSetViewports(1, &viewport);		  // ビューポート矩形
	commandList->RSSetScissorRects(1, &scissorRect); // シザー矩形

	// **バックバッファの取得**
	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> backBuffer = GetBackBuffer(backBufferIndex);
	ComPtr<ID3D12Resource> depthBuffer = GetDepthStencilResource();

	// **スワップチェインのバリア (`PRESENT` → `RENDER_TARGET`)**
	TransitionResource(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// **深度バッファのバリア (`DEPTH_WRITE` → `PIXEL_SHADER_RESOURCE`)**
	TransitionResource(depthBuffer.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// 画面をクリア
	ClearWindow();
}


/// -------------------------------------------------------------
///							描画終了処理
/// -------------------------------------------------------------
void DirectXCommon::EndDraw()
{
	HRESULT hr{};

	// **バックバッファの取得**
	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();
	ComPtr<ID3D12Resource> backBuffer = GetBackBuffer(backBufferIndex);
	backBuffer->SetName(L"BackBuffer"); // 名前をつける

	// **スワップチェインのバリア (`RENDER_TARGET` → `PRESENT`)**
	TransitionResource(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);


	// 🔹 **バックバッファを再度 RENDER_TARGET に変更**
	TransitionResource(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	// 🔹 **ImGui の描画をここで行う**
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandManager_->GetCommandList());

	// 🔹 **再度 `RENDER_TARGET → PRESENT` に変更**
	TransitionResource(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	// コマンド完了まで待つ
	commandManager_->ExecuteAndWait();
	fenceManager_->Signal(commandManager_->GetCommandQueue());
	fenceManager_->Wait();

	//GPUとOSに画面の交換を行うよう通知する
	swapChain_->GetSwapChain()->Present(1, 0); // VSync 有効（FPSを同期）-（0, 0）で無効（最大FPSで動作）

	// FPSカウント
	fpsCounter_.EndFrame();
}


/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void DirectXCommon::Finalize()
{
	// フェンスとイベントの解放
	fenceManager_->Finalize();
	device_.reset();
	swapChain_.reset();
}


/// -------------------------------------------------------------
///						バッファを取得
/// -------------------------------------------------------------
ComPtr<ID3D12Resource> DirectXCommon::GetBackBuffer(uint32_t index)
{
	ComPtr<ID3D12Resource> backBuffer = nullptr;
	HRESULT hr = swapChain_->GetSwapChain()->GetBuffer(index, IID_PPV_ARGS(&backBuffer));
	assert(SUCCEEDED(hr));
	return backBuffer.Get();
}


#pragma region デバッグレイヤーと警告時に停止処理
/// -------------------------------------------------------------
///					デバッグレイヤーの表示
/// -------------------------------------------------------------
void DirectXCommon::DebugLayer()
{
	// デバッグレイヤーをオンに
#ifdef _DEBUG
	ComPtr <ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
}


/// -------------------------------------------------------------
///					エラー・警告時の処理
/// -------------------------------------------------------------
void DirectXCommon::ErrorWarning()
{
	// エラー・警告、すなわち停止
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		//全部の情報を出す
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			//Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}
#endif // _DEBUG
}
#pragma endregion


/// -------------------------------------------------------------
///				　リソース遷移の管理する処理
/// -------------------------------------------------------------
void DirectXCommon::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	if (stateBefore == stateAfter) return;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandManager_->GetCommandList()->ResourceBarrier(1, &barrier);
}


/// -------------------------------------------------------------
///					画面全体のクリア処理
/// -------------------------------------------------------------
void DirectXCommon::ClearWindow()
{
	auto commandList = commandManager_->GetCommandList();

	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();

	// RTVとDSVの取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(backBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(0);

	// **深度バッファを DEPTH_WRITE に変更**
	ComPtr<ID3D12Resource> depthBuffer = GetDepthStencilResource();
	TransitionResource(depthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// 描画先のRTVとDSVを設定
	commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

	// 画面をクリア
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
}


/// -------------------------------------------------------------
///					　RTVとDSVの初期化処理
/// -------------------------------------------------------------
void DirectXCommon::InitializeRTVAndDSV()
{
	// DSVの初期化
	DSVManager::GetInstance()->Initialize(this);

	// 深度バッファリソースの作成
	D3D12_RESOURCE_DESC depthDesc{};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Width = kClientWidth;
	depthDesc.Height = kClientHeight;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT); // 🔹 ローカル変数を作成
	HRESULT result = device_->GetDevice()->CreateCommittedResource(
		&heapProps,  // ✅ ローカル変数のアドレスを渡す
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthStencilResource)
	);
	assert(SUCCEEDED(result) && "Failed to create Depth Stencil Buffer!");

	// DSVの作成
	uint32_t dsvIndex = DSVManager::GetInstance()->Allocate();
	DSVManager::GetInstance()->CreateDSVForDepthBuffer(dsvIndex, depthStencilResource.Get());

	// RTVの初期化
	RTVManager::GetInstance()->Initialize(this);

	// スワップチェインのRTVを作成
	for (uint32_t i = 0; i < 2; i++)
	{
		uint32_t rtvIndex = RTVManager::GetInstance()->Allocate();
		RTVManager::GetInstance()->CreateRTVForTexture2D(rtvIndex, swapChain_->GetSwapChainResources(i));
	}
}

