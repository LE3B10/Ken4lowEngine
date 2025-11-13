#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

/// -------------------------------------------------------------
///			DirectX12のフェンスを管理するクラス
/// -------------------------------------------------------------
class DX12FenceManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// フェンスの初期化処理を行います。<br/>
	/// ・ID3D12Device::CreateFence() でフェンスを生成<br/>
	/// ・CreateEvent() で GPU 完了待ち用のイベントオブジェクトを作成<br/>
	/// します。失敗した場合は assert で停止します。
	/// </summary>
	/// <param name="device">フェンスを生成するための ID3D12Device。</param>
	void Initialize(ID3D12Device* device);

	/// <summary>
	/// フェンスの終了処理を行います。<br/>
	/// ・イベントハンドルが有効なら CloseHandle() でクローズ<br/>
	/// ・ID3D12Fence を Reset() で解放<br/>
	/// します。アプリ終了時や DirectX 解放時に呼び出します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// GPU にフェンスシグナルを送信します。<br/>
	/// ・内部カウンタ fenceValue_ をインクリメントし、<br/>
	/// ・ID3D12CommandQueue::Signal() を呼び出して GPU キューにシグナルを送ります。<br/>
	/// この値を基準に Wait() 側で完了待ちを行います。
	/// </summary>
	/// <param name="commandQueue">シグナルを送る対象のコマンドキュー。</param>
	void Signal(ID3D12CommandQueue* commandQueue);

	/// <summary>
	/// GPU の処理が完了するまで CPU を待機させます。<br/>
	/// ・fence_->GetCompletedValue() が fenceValue_ 未満なら、<br/>
	/// 　SetEventOnCompletion() でイベントとフェンス値を関連付けて、<br/>
	/// 　WaitForSingleObject() で完了までブロックします。<br/>
	/// それ以外の場合は即座に復帰します。
	/// </summary>
	void Wait();

private: /// ---------- メンバ変数 ---------- ///

	// GPU と CPU の同期に使用するフェンスオブジェクト
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	// フェンス完了通知用のイベントハンドル
	HANDLE fenceEvent_ = nullptr;

	// フェンスの現在値を管理するカウンタ
	UINT64 fenceValue_ = 0;
};
