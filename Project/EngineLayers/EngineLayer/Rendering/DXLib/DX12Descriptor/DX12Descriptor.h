#pragma once
#include "DX12Include.h"
#include <cstdint>

// 最大SRV数（最大テクスチャ枚数）
static const uint32_t kMaxSRVCount = 512;

class SRVManager;

/// -------------------------------------------------------------
///				デスクリプタヒープの生成クラス
/// -------------------------------------------------------------
class DX12Descriptor
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2, uint32_t width, uint32_t height);

	// DescriptorHeapを生成する
	ComPtr <ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

	// DepthStencilTextureを生成する
	ComPtr <ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

	// レンダーターゲットビューの生成
	void GenerateRTV(ID3D12Device* device, ID3D12Resource* swapChainResoursec1, ID3D12Resource* swapChainResoursec2);

	// デプスステンシルビューの生成
	void GenerateDSV(ID3D12Device* device, uint32_t width, uint32_t height);

	// レンダーターゲットビューの生成 (新規追加)
	void CreateRTVForTexture2D(ID3D12Device* device, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

public: /// ---------- ゲッター ---------- ///

	ID3D12DescriptorHeap* GetDSVDescriptorHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandles(uint32_t num);
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle();
	ID3D12Resource* GetDepthStencilBuffer() const { return depthStencilResource.Get(); }
	// デスクリプタサイズ取得用のゲッタ
	uint32_t GetDescriptorSizeRTV() const { return descriptorSizeRTV; }
	uint32_t GetDescriptorSizeDSV() const { return descriptorSizeDSV; }


private: /// ---------- メンバ変数 ---------- ///

	SRVManager* srvManager_ = nullptr;

	ID3D12Device* device_ = nullptr;

	ComPtr <ID3D12DescriptorHeap> rtvDescriptorHeap;
	ComPtr <ID3D12DescriptorHeap> dsvDescriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];

	ComPtr <ID3D12Resource> depthStencilResource;
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;


	// デスクリプタのサイズ
	uint32_t descriptorSizeRTV = 0;
	uint32_t descriptorSizeDSV = 0;
};

