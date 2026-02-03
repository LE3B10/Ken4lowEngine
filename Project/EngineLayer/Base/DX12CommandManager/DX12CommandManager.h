#pragma once
#include "DX12Include.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DX12FenceManager;

/// -------------------------------------------------------------
///			DirectX12のコマンド周りを管理するクラス
/// -------------------------------------------------------------
class DX12CommandManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// コマンド周りの初期化処理を行います。<br/>
	/// ・ID3D12Device::CreateCommandAllocator() でコマンドアロケータを生成<br/>
	/// ・ID3D12Device::CreateCommandList() でグラフィックスコマンドリストを生成<br/>
	/// ・ID3D12Device::CreateCommandQueue() でコマンドキューを生成<br/>
	/// という流れで、描画コマンドを投げるための基本オブジェクトを用意します。<br/>
	/// いずれかが失敗した場合は assert でアプリを停止します。
	/// </summary>
	/// <param name="device">コマンドまわりのオブジェクトを生成するための ID3D12Device。</param>
	void Initialize(ID3D12Device* device);

	void Finalize();

	/// <summary>
	/// 指定したリソースのステート遷移をコマンドリストに記録します。<br/>
	/// ・stateBefore と stateAfter が同じ場合は何もしません。<br/>
	/// ・D3D12_RESOURCE_BARRIER_TYPE_TRANSITION を設定し、<br/>
	/// 　全サブリソース(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)を対象に ResourceBarrier を発行します。<br/>
	/// 実際のバリア適用タイミングは、ExecuteCommandLists で GPU がコマンドリストを実行したときになります。
	/// </summary>
	/// <param name="resource">ステートを変更したいターゲットリソース。</param>
	/// <param name="stateBefore">遷移前のリソースステート。</param>
	/// <param name="stateAfter">遷移後のリソースステート。</param>
	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	/// <summary>
	/// コマンドリストの実行と完了待ちを行います。<br/>
	/// 処理の流れ：<br/>
	/// 1. commandList_->Close() でコマンドリストをクローズ<br/>
	/// 2. commandQueue_->ExecuteCommandLists() で GPU にコマンドリストを実行させる<br/>
	/// 3. fenceManager_ が設定されていれば、Signal() → Wait() で GPU 完了まで待機<br/>
	/// 4. 次のフレームに備えて、commandAllocator_->Reset() と commandList_->Reset() で再利用可能な状態に戻す<br/>
	/// という一連の処理をまとめて行います。
	/// </summary>
	void ExecuteAndWait();

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// フェンスマネージャをセットします。<br/>
	/// ExecuteAndWait() 内で GPU 完了を待機する際に使用されます。
	/// </summary>
	/// <param name="fenceManager">同期に使用する DX12FenceManager。</param>
	void SetFenceManager(DX12FenceManager* fenceManager) { fenceManager_ = fenceManager; }

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 使用中のグラフィックスコマンドリストを取得します。<br/>
	/// 描画コマンド・リソースバリア・クリア命令などを積むときに使用します。
	/// </summary>
	/// <returns>内部で保持している ID3D12GraphicsCommandList。</returns>
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

	/// <summary>
	/// コマンドアロケータを取得します。<br/>
	/// 必要に応じて外部で Reset する場合などに使用できます。
	/// </summary>
	/// <returns>内部で保持している ID3D12CommandAllocator。</returns>
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }

	/// <summary>
	/// コマンドキューを取得します。<br/>
	/// 外部で追加の ExecuteCommandLists や Signal を行いたいときに使用します。
	/// </summary>
	/// <returns>内部で保持している ID3D12CommandQueue。</returns>
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	DX12FenceManager* fenceManager_ = nullptr; // フェンスマネージャーのポインタ

	// コマンド周りの基本オブジェクト
	ComPtr<ID3D12CommandAllocator> commandAllocator_; // コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList> commandList_;   // グラフィックスコマンドリスト
	ComPtr<ID3D12CommandQueue> commandQueue_;		  // コマンドキュー
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};	  // コマンドキューの設定情報
};

} // namespace Ken4lowEngine
