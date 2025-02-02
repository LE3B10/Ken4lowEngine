#pragma once
#include "DX12Include.h"
#include <mutex>
#include <cstdint>
#include <stdexcept>
#include <queue>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///			　Shader Resource Viewを管理するクラス
/// -------------------------------------------------------------
class SRVManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static SRVManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// SRV生成（テクスチャ用）
	void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	// SRV生成（Structered Buffer用）
	void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	// ヒープセットコマンド
	void PreDraw();

	// SRVセットコマンド
	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	// 確保
	uint32_t Allocate();

	// 解放処理
	void Free(uint32_t srvIndex);

	SRVManager() = default;
	~SRVManager() = default;

public: /// ---------- ゲッター ---------- ///

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap.Get(); }

	ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

	// CPUデスクリプタヒープを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	// GPUデスクリプタヒープを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	uint32_t GetDescriptorSize() const { return descriptorSize; }

	uint32_t GetkMaxSRVCount() const { return kMaxSRVCount; }

private: /// ---------- メンバ変数 ---------- ///

	// DirectXCommonのポインタ
	DirectXCommon* dxCommon_ = nullptr;

	// 最大SRV数（最大テクスチャ枚数）
	static const uint32_t kMaxSRVCount = 512;

	// SRV用のデスクリプタサイズ
	uint32_t descriptorSize = 0;

	// SRV用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	// 次に使用するSRVインデックス
	uint32_t useIndex = 0;

	// スレッドセーフ用
	std::mutex allocationMutex;

	// 空きインデックスのリスト
	std::queue<uint32_t> freeIndices;

private:

	SRVManager(const SRVManager&) = delete;
	SRVManager& operator=(const SRVManager&) = delete;
};

