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

	// 初期化処理
	void Initialize(ID3D12Device* device);

	// 終了処理
	void Finalize();

	// GPUにシグナルを送る
	void Signal(ID3D12CommandQueue* commandQueue);

	// GPUの処理が完了するまで待機
	void Wait();

private: /// ---------- メンバ変数 ---------- ///

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	HANDLE fenceEvent_ = nullptr;
	UINT64 fenceValue_ = 0;
};
