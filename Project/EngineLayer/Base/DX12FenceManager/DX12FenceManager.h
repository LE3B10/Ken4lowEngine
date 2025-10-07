#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

class DX12FenceManager
{
public:
    void Initialize(ID3D12Device* device);
    void Finalize();

    void Signal(ID3D12CommandQueue* commandQueue);
    void Wait();

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;
};
