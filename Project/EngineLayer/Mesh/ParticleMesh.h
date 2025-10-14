#pragma once
#include <DX12Include.h>
#include "VertexData.h"

#include <vector>

/// -------------------------------------------------------------
///				　	　パーティクルメッシュクラス
///	-------------------------------------------------------------
class ParticleMesh
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// リングの頂点データを生成
	void InitializeRing();

	// シリンダーの頂点データの初期化処理
	void InitializeCylinder();

	// 星型の頂点データの初期化処理
	void InitializeStar();

	// スモークの頂点データの初期化処理
	void InitializeSmoke();

	// 描画処理
	void Draw(UINT instanceCount);

public: /// ---------- ゲッター ---------- ///

	// 頂点バッファビューの取得
	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }

	// インデックスバッファビューの取得
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }

	// インデックスの有無
	bool HasIndex() const { return hasIndex_; }

private: /// ---------- メンバ関数 ---------- ///

	// 頂点データの生成
	void CreateVertexBuffer();

private: /// ---------- メンバ変数 ---------- ///

	// 自前のジオメトリ（Particleはファイルロードしない）
	std::vector<VertexData> vertices;
	std::vector<uint32_t>   indices;   // ない場合もある

	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	VertexData* vertexData_ = nullptr;

	// インデックス対応
	ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	bool hasIndex_ = false;
};

