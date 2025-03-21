#pragma once
#include "DX12Include.h"
#include <mutex>
#include <cstdint>
#include <stdexcept>
#include <queue>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///			レンダーターゲットビュー（RTV）を管理するクラス
/// -------------------------------------------------------------
class RTVManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static RTVManager* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon, uint32_t maxRTVCount = kDefaultMaxRTVCount_);

	// 空いているRTVのインデックスを確保する
	uint32_t Allocate();

	// 指定したRTVのインデックスを解放する
	void Free(uint32_t rtvIndex);

	// 指定されたテクスチャリソースのRTVを作成する
	void CreateRTVForTexture2D(uint32_t rtvIndex, ID3D12Resource* resource);

public: /// ---------- ゲッター ---------- ///

	// ディスクリプタヒープを取得
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

	// 指定インデックスのCPUデスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	uint32_t descriptorSize_ = 0;  // RTVデスクリプタのサイズ
	static const uint32_t kDefaultMaxRTVCount_ = 128; // デフォルトの最大RTV数
	uint32_t maxRTVCount_ = kDefaultMaxRTVCount_; // RTVの最大数
	uint32_t useIndex_ = 0;        // 次に使用するRTVのインデックス

	std::mutex allocationMutex_;  // スレッドセーフのためのミューテックス
	std::queue<uint32_t> freeIndices_;  // 解放済みのRTVインデックスリスト

	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;  // RTV用デスクリプタヒープ

private: /// ---------- コピー禁止 ---------- ///

	RTVManager() = default;
	~RTVManager() = default;
	RTVManager(const RTVManager&) = delete;
	RTVManager& operator=(const RTVManager&) = delete;
};

