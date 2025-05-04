#pragma once
#include <DX12Include.h>
#include <ModelData.h>


class ParticleMesh
{
public: /// ---------- メンバ関数 ---------- ///
	
	// 初期化処理
	void Initialize();

	// リングの頂点データを生成
	void CreateVertexData();

	// シリンダーの頂点データの初期化処理
	void InitializeCylinder();

	// 描画処理
	void Draw(UINT num);

	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }

private: /// ---------- メンバ変数 ---------- ///

	ModelData modelData_; // 頂点データなど
	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	VertexData* vertexData_ = nullptr; // 頂点データ
};

