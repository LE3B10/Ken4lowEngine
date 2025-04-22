#pragma once
#include "DX12Include.h"
#include "VertexData.h"

#include <cmath>
#include <numbers>
#include <vector>

/// -------------------------------------------------------------
///				　	　メッシュデータクラス
/// -------------------------------------------------------------
class Mesh
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::vector<VertexData>& modelVertices);

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

private: /// ---------- メンバ変数 ---------- ///

	// 頂点バッファ
	ComPtr<ID3D12Resource> vertexResource;

	// 頂点リソースにデータを書き込む
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	// 頂点リソース内のデータを指すポインタ
	std::vector<VertexData> vertices;
};

