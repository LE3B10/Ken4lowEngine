#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						　初期化処理
	/// -------------------------------------------------------------
	void DX12CommandManager::Initialize(ID3D12Device* device)
	{
		HRESULT hr{};
		commandListSubmitted_ = false;

		//コマンドロケータを生成する
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
		commandAllocator_->SetName(L"Main Command Allocator");
		//コマンドアロケータの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));

		//コマンドリストを生成する
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
		commandList_->SetName(L"Main Command List");
		//コマンドリストの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));

		//コマンドキューを生成する
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 追加
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
		commandQueue_->SetName(L"Main Command Queue");
		//コマンドキューの生成がうまくいかなかったので起動できない
		assert(SUCCEEDED(hr));
	}

	void DX12CommandManager::Finalize()
	{
		assert(!commandListSubmitted_ && "DirectXCommon must finish submitted GPU work before finalizing the command manager.");
		commandQueue_.Reset();
		commandList_.Reset();
		commandAllocator_.Reset();
	}

	/// -------------------------------------------------------------
	///				　リソースの状態遷移を行う
	/// -------------------------------------------------------------
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

	/// -------------------------------------------------------------
	///				　コマンド実行と待機
	/// -------------------------------------------------------------
	void DX12CommandManager::ExecuteAndWait()
	{
		Execute();
		WaitAndReset();
	}

	void DX12CommandManager::Execute()
	{
		assert(!commandListSubmitted_ && "WaitAndReset must complete before submitting the command list again.");
		if (commandListSubmitted_) return;

		const HRESULT hr = commandList_->Close();
		assert(SUCCEEDED(hr));
		if (FAILED(hr)) return;

		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		commandListSubmitted_ = true; // GPU参照中はAllocatorとCommandListをResetしない。
	}

	void DX12CommandManager::WaitAndReset()
	{
		if (!commandListSubmitted_) return;
		assert(fenceManager_ && "A fence manager is required before reusing a submitted command allocator.");
		if (!fenceManager_) return;

		fenceManager_->Signal(commandQueue_.Get());
		fenceManager_->Wait();

		const HRESULT allocatorResult = commandAllocator_->Reset();
		assert(SUCCEEDED(allocatorResult));
		if (FAILED(allocatorResult)) return;
		const HRESULT commandListResult = commandList_->Reset(commandAllocator_.Get(), nullptr);
		assert(SUCCEEDED(commandListResult));
		if (SUCCEEDED(commandListResult)) commandListSubmitted_ = false;
	}

} // namespace Ken4lowEngine
