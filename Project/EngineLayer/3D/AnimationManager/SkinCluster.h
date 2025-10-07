#pragma once
#include "DX12Include.h"
#include "ModelData.h"
#include "Matrix4x4.h"

#include <span>
#include <vector>
#include <utility>

class Skeleton;

class SkinCluster
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~SkinCluster();

	// 初期化処理
	void Initialize(const ModelData& modelData, Skeleton& skeleton);

	// スケルトンからパレット行列を更新
	void UpdatePaletteMatrix(Skeleton& skeleton);

	// GPU用ハンドルやビューの取得
	const D3D12_VERTEX_BUFFER_VIEW& GetInfluenceBufferView() const { return influenceBufferView_; }
	const std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>& GetPaletteSrvHandle() const { return paletteSrvHandle_; }

	// Compute(Dispatch)用にUAVManagerヒープ上のGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetPaletteSrvOnUAVHeap() const { return paletteSrvGpuOnUavHeap_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetInfluenceSrvOnUAVHeap() const { return influenceSrvGpuOnUavHeap_; }

private: /// ---------- メンバ変数 ---------- ///

	std::vector<Matrix4x4> inverseBindPoseMatrices_; // パレット行列

	// influence（頂点ごとのデータ）
	ComPtr<ID3D12Resource> influenceResource_; // 頂点バッファリソース
	D3D12_VERTEX_BUFFER_VIEW influenceBufferView_{}; // 頂点バッファビュー
	std::span<VertexInfluence> mappedInfluenceData_; // マッピングしたデータ

	// palette（ジョイント行列の配列）
	ComPtr<ID3D12Resource> paletteResource_; // パレットリソース
	std::span<WellForGPU> mappedPalette_; // マッピングしたデータ
	uint32_t paletteSrvIndex_ = UINT32_MAX; // SRVのインデックス
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle_; // SRVのハンドル

	ComPtr<ID3D12Resource> influenceResourceDefault_; // CS/VS が読む DEFAULT 常駐
	ComPtr<ID3D12Resource> paletteResourceDefault_;  // CS/VS が読む用（毎フレ Copy で更新）
	uint32_t paletteSrvIndexOnUavHeap_ = UINT32_MAX;
	uint32_t influenceSrvIndexOnUavHeap_ = UINT32_MAX;
	D3D12_GPU_DESCRIPTOR_HANDLE paletteSrvGpuOnUavHeap_{};
	D3D12_GPU_DESCRIPTOR_HANDLE influenceSrvGpuOnUavHeap_{};
};

