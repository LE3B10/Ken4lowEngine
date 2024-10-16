#include "DirectXCommon.h"

#include <cassert>

#include "WinApp.h"

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
	descriptor = std::make_unique<DX12Descriptor>();

	kClientWidth = Width;
	kClientHeight = Height;

	// FPS固定初期化処理
	InitializeFixFPS();

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

	// RTV, DSVの初期化
	descriptor->Initialize(device_->GetDevice(), swapChain_->GetSwapChainResources(0), swapChain_->GetSwapChainResources(1), Width, Height);

}



/// -------------------------------------------------------------
///							描画開始処理
/// -------------------------------------------------------------
void DirectXCommon::BeginDraw()
{
	// これから書き込むバックバッファのインデックスを取得
	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();

	// バリアで書き込み可能に変更
	ChangeBarrier();

	// 画面をクリア
	ClearWindow();

	// ビューポート矩形の設定
	viewport = D3D12_VIEWPORT(0.0f, 0.0f, (float)kClientWidth, (float)kClientHeight);
	commandList->RSSetViewports(1, &viewport);

	// シザリング矩形の設定
	scissorRect = D3D12_RECT(0, 0, kClientWidth, kClientHeight);
	commandList->RSSetScissorRects(1, &scissorRect);

	// 形状を設定。PSOに設定るものとはまた別。同じものを設定るすると考える
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}



/// -------------------------------------------------------------
///							描画終了処理
/// -------------------------------------------------------------
void DirectXCommon::EndDraw()
{
	HRESULT hr{};

	// 画面に描く処理はすべて終わり、画面に移すので、状態を遷移
	// 今回はRenderTargetからPresentにする
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	
	// TransitionBarirrerを張る
	commandList->ResourceBarrier(1, &barrier);

	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { commandList.Get() };

	// GPUに対して積まれたコマンドを実行
	commandQueue->ExecuteCommandLists(1, commandLists);

	//GPUとOSに画面の交換を行うよう通知する
	swapChain_->GetSwapChain()->Present(1, 0);

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

	// FPS固定更新処理
	UpdateFixFPS();

	// 次のフレーム用のコマンドリストを準備（コマンドリストのリセット）
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}



/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void DirectXCommon::Finalize()
{
	CloseHandle(fenceEvent);

	device_.reset();
	swapChain_.reset();
	descriptor.reset();
}



/// -------------------------------------------------------------
///							ゲッター
/// -------------------------------------------------------------
ID3D12Device* DirectXCommon::GetDevice() const
{
	return device_->GetDevice();
}

ID3D12GraphicsCommandList* DirectXCommon::GetCommandList() const
{
	return commandList.Get();
}

ID3D12DescriptorHeap* DirectXCommon::GetSRVDescriptorHeap() const
{
	return descriptor->GetSRVDescriptorHeap();
}

IDxcUtils* DirectXCommon::GetIDxcUtils() const
{
	return dxcUtils.Get();
}

IDxcCompiler3* DirectXCommon::GetIDxcCompiler() const
{
	return dxcCompiler.Get();
}

IDxcIncludeHandler* DirectXCommon::GetIncludeHandler() const
{
	return includeHandler.Get();
}

DXGI_SWAP_CHAIN_DESC1& DirectXCommon::GetSwapChainDesc()
{
	// TODO: return ステートメントをここに挿入します
	return swapChain_->GetSwapChainDesc();
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
	hr = device_->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューを生成する
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
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
}



/// -------------------------------------------------------------
///				ビューポート矩形の初期化処理
/// -------------------------------------------------------------
void DirectXCommon::InitializeViewport()
{
	//クライアント領域のサイズと一緒に画面全体に表示
	viewport.Width = (float)kClientWidth;
	viewport.Height = (float)kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}



/// -------------------------------------------------------------
///				シザリング矩形の初期化処理
/// -------------------------------------------------------------
void DirectXCommon::InitializeScissoring()
{
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;
}



/// -------------------------------------------------------------
///					DXCコンパイラーの生成
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
///			バリアで書き込み可能に変更する処理
/// -------------------------------------------------------------
void DirectXCommon::ChangeBarrier()
{
	// 今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	// Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを春対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChain_->GetSwapChainResources(backBufferIndex);
	// 遷移前（現在）のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);
}



/// -------------------------------------------------------------
///					画面全体のクリア処理
/// -------------------------------------------------------------
void DirectXCommon::ClearWindow()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};

	for (uint32_t i = 0; i < 2; i++)
	{
		rtvHandles[i] = descriptor->GetRTVHandles(i);
	}

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = descriptor->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();

	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

	//指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };	//青っぽい色。RGBAの順
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

	//指定した深度で画面全体をクリアする
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}



/// -------------------------------------------------------------
///					FPS固定初期化処理
/// -------------------------------------------------------------
void DirectXCommon::InitializeFixFPS()
{
	// 現在の時間を記録する - 逆行しないタイマー
	reference_ = std::chrono::steady_clock::now();
}



/// -------------------------------------------------------------
///					FPS固定更新処理
/// -------------------------------------------------------------
void DirectXCommon::UpdateFixFPS()
{
	// 1/60秒ピッタリの時間
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

	// 1/60秒より技化に短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

	// 現在の時間を取得する
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	// 前回の記録からの経過時間を取得する
	std::chrono::microseconds elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	// 1/60秒（よりわずかに短い時間）経っていない場合
	if (elapsed < kMinTime)
	{
		// 1/60秒経過するまで微小なスリープを繰り返す
		while (std::chrono::steady_clock::now() - reference_ < kMinTime)
		{
			// 1マイクロ病スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}

	// 現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}
