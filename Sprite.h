#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "ResourceManager.h"
#include "VertexData.h"

#include <array>
#include <memory>

/// ---------- スプライトの頂点数 ( Vertex, Index ) ----------- ///
static const UINT kVertexNum = 6;
static const UINT kIndexNum = 4;

/// -------------------------------------------------------------
///						スプライトクラス
/// -------------------------------------------------------------
class Sprite
{
public: /// ---------- スプライトデータの構造体 ---------- ///

	struct SpriteData
	{
		/// ---------- 頂点バッファデータ ---------- ///

		// 頂点リソース
		Microsoft::WRL::ComPtr <ID3D12Resource> vertexResourceSprite;
		// 頂点バッファービュー
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
		// インデックス頂点バッファリソース
		Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSprite;
		// インデックスバッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
		// 頂点バッファデータ
		VertexData* vertexDataSprite = nullptr;
		// インデックスデータ
		uint32_t* indexDataSprite = nullptr;

		/// ---------- マテリアルデータ ---------- ///

		// マテリアルリソース
		Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceSprite;
		// マテリアルデータ
		Material* materialDataSprite = nullptr;

		/// ---------- 座標変換データ ---------- ///

		// 座標変換行列リソース
		Microsoft::WRL::ComPtr <ID3D12Resource> transformationMatrixResourceSprite;
		//	座標変換行列データ
		TransformationMatrix* transformationMatrixDataSprite = nullptr;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

public: /// ---------- セッター ---------- ///

	// VBV - IBV - CBVの設定（スプライト用）
	void SetSpriteBufferData(ID3D12GraphicsCommandList* commandList);
	
	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList);

private: /// ---------- メンバ変数 ---------- ///

	// スプライトデータ
	std::unique_ptr<SpriteData> spriteData_;
	// スプライトメッシュ生成
	std::unique_ptr<SpriteData> CreateSpriteData(UINT vertexCount, UINT indexCount);

	// CreateBuffer用
	ResourceManager createBuffer_;

};

