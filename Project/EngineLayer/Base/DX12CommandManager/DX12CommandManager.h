#pragma once
#include "DX12Include.h"

class DX12FenceManager;

class DX12CommandManager
{
public:
	void Initialize(ID3D12Device* device);

	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	void SetFenceManager(DX12FenceManager* fenceManager);
	void ExecuteAndWait(); // コマンド実行と待機

	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

private:

	DX12FenceManager* fenceManager_ = nullptr; // フェンスマネージャーのポインタ

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
};
