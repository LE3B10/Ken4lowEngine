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
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(ID3D12Device* device, const ModelData& modelData);

public: /// ---------- ゲッター ---------- ///

	// --- 新規: 複数サブメッシュ対応のAPI ---
	size_t GetSubmeshCount() const { return subMeshes_.size(); }
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(size_t i) const { return subMeshes_[i].vertexBufferView_; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t i)  const { return subMeshes_[i].indexBufferView_; }
	ID3D12Resource* GetVertexBufferResource(size_t i) const { return subMeshes_[i].vertexResource_.Get(); }
	ID3D12Resource* GetIndexBufferResource(size_t i)  const { return subMeshes_[i].indexResource_.Get(); }

	// --- 互換: 既存シングルAPI（先頭サブメッシュを返す）---
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return subMeshes_.front().vertexBufferView_; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView()  const { return subMeshes_.front().indexBufferView_; }
	ID3D12Resource* GetVertexBufferResource() const { return subMeshes_.front().vertexResource_.Get(); }
	ID3D12Resource* GetIndexBufferResource()  const { return subMeshes_.front().indexResource_.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	std::vector<SubMeshGPU> subMeshes_;
};

