#include "DX12FenceManager.h"
#include <cassert>

/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void DX12FenceManager::Initialize(ID3D12Device* device)
{
	// フェンスの生成
	HRESULT hr = S_OK;
	hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));

	// イベントの生成
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

/// -------------------------------------------------------------
///						　終了処理
/// -------------------------------------------------------------
void DX12FenceManager::Finalize()
{
	// イベントが有効なら
	if (fenceEvent_)
	{
		// イベントハンドルを閉じる
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	fence_.Reset();
}

/// -------------------------------------------------------------
///				　	GPUにシグナルを送る
/// -------------------------------------------------------------
void DX12FenceManager::Signal(ID3D12CommandQueue* commandQueue)
{
	// 次の値に更新してシグナルを送る
	fenceValue_++;
	HRESULT hr = S_OK;
	hr = commandQueue->Signal(fence_.Get(), fenceValue_);
	assert(SUCCEEDED(hr));
}

/// -------------------------------------------------------------
///				    GPUの処理が完了するまで待機
/// -------------------------------------------------------------
void DX12FenceManager::Wait()
{
	// GPUの処理が完了していなければ待機する
	if (fence_->GetCompletedValue() < fenceValue_)
	{
		HRESULT hr = S_OK;
		hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		assert(SUCCEEDED(hr));
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}
