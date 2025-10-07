#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

void DX12CommandManager::Initialize(ID3D12Device* device)
{
	HRESULT hr{};

	//コマンドロケータを生成する
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	//コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドリストを生成する
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューを生成する
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 追加
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));
}

void DX12CommandManager::ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
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

void DX12CommandManager::SetFenceManager(DX12FenceManager* fenceManager)
{
	fenceManager_ = fenceManager;
}

void DX12CommandManager::ExecuteAndWait()
{
	HRESULT hr{};

	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	ComPtr<ID3D12CommandList> commandLists[] = { commandList_.Get() };

	// GPUに対して積まれたコマンドを実行
	commandQueue_->ExecuteCommandLists(1, commandLists->GetAddressOf());

	// 🔹 GPUの完了を待つ
	if (fenceManager_) {
		fenceManager_->Signal(commandQueue_.Get());
		fenceManager_->Wait();
	}

	// 次のフレーム用のコマンドリストを準備（コマンドリストのリセット）
	hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));

	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));
}
