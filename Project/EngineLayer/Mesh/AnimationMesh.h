#pragma once
#include "DX12Include.h"
#include "ModelData.h"

#include <memory>

/// -------------------------------------------------------------
///				　	　アニメーションメッシュクラス
/// -------------------------------------------------------------
class AnimationMesh
{
private: /// ---------- 型定義 ---------- ///

	// サブメッシュ一つ分のGPUリソースをまとめた構造体
	struct SubMeshGPU
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 頂点バッファリソース
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;	// インデックスバッファリソース
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};			// 頂点バッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView_{};				// インデックスバッファビュー
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// アニメーションメッシュの初期化処理
	/// </summary>
	/// <param name="device">デバイス</param>
	/// <param name="modelData">モデルデータ</param>
	void Initialize(ID3D12Device* device, const ModelData& modelData);

public: /// ---------- ゲッター ---------- ///

	// サブメッシュ数を取得
	size_t GetSubmeshCount() const { return subMeshes_.size(); }

	// 頂点バッファビューを取得
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(size_t i) const { return subMeshes_[i].vertexBufferView_; }

	// インデックスバッファビューを取得
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t i)  const { return subMeshes_[i].indexBufferView_; }

	// 頂点バッファリソースを取得
	ID3D12Resource* GetVertexBufferResource(size_t i) const { return subMeshes_[i].vertexResource_.Get(); }

	// インデックスバッファリソースを取得
	ID3D12Resource* GetIndexBufferResource(size_t i)  const { return subMeshes_[i].indexResource_.Get(); }

public: /// ---------- ゲッター（サブメッシュ0固定版） ---------- ///

	// 頂点バッファビューを取得
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return subMeshes_.front().vertexBufferView_; }

	// インデックスバッファビューを取得
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()  const { return subMeshes_.front().indexBufferView_; }

	// 頂点バッファリソースを取得
	ID3D12Resource* GetVertexBufferResource() const { return subMeshes_.front().vertexResource_.Get(); }

	// インデックスバッファリソースを取得
	ID3D12Resource* GetIndexBufferResource()  const { return subMeshes_.front().indexResource_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	// サブメッシュごとのGPUリソース配列
	std::vector<SubMeshGPU> subMeshes_;
};

