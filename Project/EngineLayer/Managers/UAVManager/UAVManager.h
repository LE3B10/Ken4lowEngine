#pragma once
#include "DX12Include.h"
#include <memory>
#include <mutex>
#include <queue>
#include <cstdint>
#include <stdexcept>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　		 UAVマネージャークラス
/// -------------------------------------------------------------
class UAVManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンスの取得
	static UAVManager* GetInstance();
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon);

	// UAVヒープのセットアップ
	void PreDispatch();

	// UAVを作成（Texture2D用）
	void CreateUAVForTexture2D(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	// UAVヒープ（=CBV_SRV_UAV型）上にSRVを作成する
	void CreateSRVForTexture2DOnThisHeap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
	
	// UAVを作成（Buffer用）
	void CreateUAVForBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT64 bufferSize);

	// UAVを作成（Structure Buffer用）
	void CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);


public: /// ---------- 容量の確保 ---------- ///

	// 確保
	uint32_t Allocate();

	// 解放処理
	void Free(uint32_t srvIndex);

	// CPUデスクリプタヒープを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	// GPUデスクリプタヒープを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのインスタンス

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc_; // デスクリプタヒープの設定
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_; // UAV用のデスクリプタヒープ
	UINT descriptorSize_ = 0; // デスクリプタサイズ
	static constexpr uint32_t kMaxUAVCount = 1024; // 最大UAV数

	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;

	// スレッドセーフ用
	std::mutex allocationMutex_;

	// 空きインデックスのリスト
	std::queue<uint32_t> freeIndices_;

private: /// ---------- コピー禁止 ---------- ///

	UAVManager() = default;
	~UAVManager() = default;
	UAVManager(const UAVManager&) = delete;
	const UAVManager& operator=(const UAVManager&) = delete;
};

