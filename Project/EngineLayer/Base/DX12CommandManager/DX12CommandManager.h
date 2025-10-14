#pragma once
#include "DX12Include.h"

/// ---------- 前方宣言 ---------- ///
class DX12FenceManager;

/// -------------------------------------------------------------
///			DirectX12のコマンド周りを管理するクラス
/// -------------------------------------------------------------
class DX12CommandManager
{
public: /// ---------- メンバ関数 ---------- ///

	// コマンド関連の初期化
	void Initialize(ID3D12Device* device);

	// リソースの状態遷移を行う
	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	// コマンド実行と待機
	void ExecuteAndWait();

public: /// ---------- セッター ---------- ///

	// フェンスマネージャーのセット
	void SetFenceManager(DX12FenceManager* fenceManager) { fenceManager_ = fenceManager; }

public: /// ---------- ゲッター ---------- ///

	// コマンドリストの取得
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

	// コマンドアロケータの取得
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }

	// コマンドキューの取得
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	DX12FenceManager* fenceManager_ = nullptr; // フェンスマネージャーのポインタ

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
};
