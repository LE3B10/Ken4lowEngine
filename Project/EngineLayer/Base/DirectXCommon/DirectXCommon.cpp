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

	kClientWidth = Width;
	kClientHeight = Height;

	// デバッグレイヤーをオンに
	DebugLayer();

	// デバイスの初期化
	device_->Initialize();

	// エラー、警告
	ErrorWarning();

	// コマンド生成
	CreateCommands();

	// スワップチェインの生成
	swapChain_->Initialize(winApp, device_->GetDXGIFactory(), commandQueue.Get(), Width, Height);

	// フェンスとイベントの生成
	CreateFenceEvent();

	// DXCコンパイラの生成
	CreateDXCCompiler();

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

	commandList_->RSSetViewports(1, &viewport);		  // ビューポート矩形
	commandList_->RSSetScissorRects(1, &scissorRect); // シザー矩形

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
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

	// 🔹 **再度 `RENDER_TARGET → PRESENT` に変更**
	TransitionResource(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);


	// コマンド完了まで待つ
	WaitCommand();

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
	CloseHandle(fenceEvent);

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
///						コマンド生成
/// -------------------------------------------------------------
void DirectXCommon::CreateCommands()
{
	HRESULT hr{};

	//コマンドロケータを生成する
	hr = device_->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	//コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドリストを生成する
	hr = device_->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューを生成する
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 追加
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device_->GetDevice()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

}


/// -------------------------------------------------------------
///					フェンスとイベントの生成
/// -------------------------------------------------------------
void DirectXCommon::CreateFenceEvent()
{
	HRESULT hr{};

	// FenceとEventを生成する
	fence = nullptr;
	fenceValue = 0;

	hr = device_->GetDevice()->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを待つためのイベントを作成する
	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
}


/// -------------------------------------------------------------
///						DXCコンパイラーの生成
/// -------------------------------------------------------------
void DirectXCommon::CreateDXCCompiler()
{
	HRESULT hr{};

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}


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

	commandList_->ResourceBarrier(1, &barrier);
}


/// -------------------------------------------------------------
///					画面全体のクリア処理
/// -------------------------------------------------------------
void DirectXCommon::ClearWindow()
{
	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();

	// RTVとDSVの取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(backBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = DSVManager::GetInstance()->GetCPUDescriptorHandle(0);

	// **深度バッファを DEPTH_WRITE に変更**
	ComPtr<ID3D12Resource> depthBuffer = GetDepthStencilResource();
	TransitionResource(depthBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// 描画先のRTVとDSVを設定
	commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

	// 画面をクリア
	float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
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


/// -------------------------------------------------------------
///						コマンドを待つ処理
/// -------------------------------------------------------------
void DirectXCommon::WaitCommand()
{
	HRESULT hr{};

	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	ComPtr<ID3D12CommandList> commandLists[] = { commandList_.Get() };

	// GPUに対して積まれたコマンドを実行
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());

	// Fenceの値を更新
	fenceValue++;

	// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);

	// Fenceの値が指定したSignal値にたどり着いているか確認する
	if (fence->GetCompletedValue() < fenceValue)
	{
		// 指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		// イベントを待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// 次のフレーム用のコマンドリストを準備（コマンドリストのリセット）
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList_->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}
