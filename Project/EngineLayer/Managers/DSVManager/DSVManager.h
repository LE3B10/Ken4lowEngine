#pragma once
#include "DX12Include.h"
#include <mutex>
#include <cstdint>
#include <stdexcept>
#include <queue>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///			デプスステンシルビュー（DSV）を管理するクラス
/// -------------------------------------------------------------
class DSVManager
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンスを取得
	static DSVManager* GetInstance();

	// 初期化処理（DSV用のデスクリプタヒープを作成）
	void Initialize(DirectXCommon* dxCommon, uint32_t maxDSVCount = kDefaultMaxDSVCount_);

	// 深度バッファのリソースを生成する
	ComPtr<ID3D12Resource> CreateDepthStencilBuffer(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_CLEAR_VALUE& outClearValue);

	// 空いているDSVのインデックスを確保する
	uint32_t Allocate();

	// 指定したDSVのインデックスを解放する
	void Free(uint32_t dsvIndex);

	// 指定された深度バッファリソースのDSVを作成する
	void CreateDSVForDepthBuffer(uint32_t dsvIndex, ID3D12Resource* depthResource);

	// 指定されたテクスチャリソースのDSVを作成する
	void CreateDSVForTexture2D(uint32_t dsvIndex, ID3D12Resource* resource);

public: /// ---------- ゲッター ---------- ///

	// デスクリプタヒープを取得
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

	// 指定インデックスのCPUデスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;  // DirectXの共通管理クラス（デバイス取得用）

	uint32_t descriptorSize_ = 0;					  // DSVデスクリプタのサイズ
	static const uint32_t kDefaultMaxDSVCount_ = 128; // デフォルト最大DSV数
	uint32_t maxDSVCount_ = kDefaultMaxDSVCount_;	  // 最大DSV数
	uint32_t useIndex_ = 0;							  // 次に使用するDSVのインデックス

	std::mutex allocationMutex_;	   // スレッドセーフのためのミューテックス
	std::queue<uint32_t> freeIndices_; // 解放済みのDSVインデックスリスト

	ComPtr<ID3D12DescriptorHeap> descriptorHeap_; // DSV用デスクリプタヒープ

private: /// ---------- コピー禁止 ---------- ///

	DSVManager() = default;
	~DSVManager() = default;
	DSVManager(const DSVManager&) = delete;
	DSVManager& operator=(const DSVManager&) = delete;
};
