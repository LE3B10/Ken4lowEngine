#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"

#include "Vector2.h"
#include "Vector4.h"

#include <array>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///					　スカイボックスクラス
/// -------------------------------------------------------------
class SkyBox
{
private: /// ---------- 構造体 ---------- ///
	
	/// ---------- 頂点数 ( Vertex, Index ) ----------- ///
	static inline const UINT kNumVertex = 24;
	static inline const UINT kNumIndex = 36;

	// マテリアルデータの構造体
	struct Material final
	{
		Vector4 color;
		Matrix4x4 uvTransform;
		float padding[3];
	};

	// 頂点データの構造体
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
	};

	// 座標変換行列データの構造体
	struct TransformationMatrix final
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& filePath);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ関数 ---------- ///

	// マテリアルデータの初期化処理
	void InitializeMaterial();

	// 頂点バッファデータの初期化
	void InitializeVertexBufferData();

	// インデックスデータの初期化
	void InitializeIndexData();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	WorldTransform worldTransform_;

	// テクスチャ番号
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;

	//スプライト用のマテリアルソースを作る
	ComPtr <ID3D12Resource> materialResource;
	Material* materialData_ = nullptr;

	ComPtr <ID3D12Resource> vertexResource;// 頂点リソースを作る
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};// 頂点バッファビューを作成する
	VertexData* vertexData = nullptr;// 頂点データを設定する
	ComPtr <ID3D12Resource> transformationMatrixResource;// TransformationMatrix用のリソース
	TransformationMatrix* transformationMatrixData = nullptr;//データを書き込む

	// インデックスバッファを作成および設定する
	ComPtr <ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t* indexData = nullptr;
};

