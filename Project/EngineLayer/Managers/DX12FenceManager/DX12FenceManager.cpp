#include "DX12FenceManager.h"
#include <cassert>

void DX12FenceManager::Initialize(ID3D12Device* device)
{
    HRESULT hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DX12FenceManager::Finalize()
{
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    fence_.Reset();
}

void DX12FenceManager::Signal(ID3D12CommandQueue* commandQueue)
{
    fenceValue_++;
    HRESULT hr = commandQueue->Signal(fence_.Get(), fenceValue_);
    assert(SUCCEEDED(hr));
}

void DX12FenceManager::Wait()
{
    if (fence_->GetCompletedValue() < fenceValue_) {
        HRESULT hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        assert(SUCCEEDED(hr));
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}
