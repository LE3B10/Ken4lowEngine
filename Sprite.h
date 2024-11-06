#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "TransformationMatrix.h"
#include "ResourceManager.h"
#include "VertexData.h"
#include "Transform.h"

#include <array>
#include <memory>

class DirectXCommon;

/// ---------- スプライトの頂点数 ( Vertex, Index ) ----------- ///
static const UINT kVertexNum = 6;
static const UINT kIndexNum = 4;

/// -------------------------------------------------------------
///						スプライトクラス
/// -------------------------------------------------------------
class Sprite
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	void DrawImGui();

	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList);

	// position　ゲッターとセッター
	const Vector2& GetPosition() const { return position_; }
	void SetPosition(const Vector2& position) { position_ = position; }

	// rotation　ゲッターとセッター
	float GetRotation() const { return rotation_; }
	void SetRotation(float rotation) { rotation_ = rotation; }

	// size ゲッターとセッター
	const Vector2& GetSize() const { return size_; }
	void SetSize(const Vector2& size) { size_ = size; }

	// 色
	const Vector4& GetColor() const { return materialDataSprite->color; }
	void SetColor(const Vector4& color) {materialDataSprite->color = color;}

	// アンカーのアクセッサー
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

public: /// ---------- セッター ---------- ///

	// VBV - IBV - CBVの設定（スプライト用）
	void SetSpriteBufferData(ID3D12GraphicsCommandList* commandList);

	//void SetTransform2D(const Transform2D transform2D) { transform2D_ = transform2D; }

private: /// ---------- メンバ変数 ---------- ///

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	void CreateMaterialResource(DirectXCommon* dxCommon);

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	void CreateVertexBufferResource(DirectXCommon* dxCommon);

	// スプライトのインデックスバッファを作成
	void CreateIndexBuffer(DirectXCommon* dxCommon);

private: /// ---------- メンバ変数 ---------- ///

	// 座標
	Vector2 position_ = { 0.0f,0.0f };
	// 回転
	float rotation_;
	// サイズ
	Vector2 size_ = { 640.0f, 360.0f };

	Vector2 anchorPoint_ = { 0.0f,0.0f };

	Vector4 color_;


private: /// ---------- メンバ変数 ---------- ///
	
	// CreateBuffer用
	ResourceManager* createBuffer_ = nullptr;

	Transform transformSprite;
	//UVTransform用の変数を用意
	Transform uvTransformSprite;

	//スプライト用のマテリアルソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> materialResourceSprite;
	Material* materialDataSprite = nullptr;

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	//Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResourceSprite;
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// 頂点データを設定する
	VertexData* vertexDataSprite = nullptr;
	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	Microsoft::WRL::ComPtr <ID3D12Resource> transformationMatrixResourceSprite;
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	// スプライトのインデックスバッファを作成および設定する
	Microsoft::WRL::ComPtr <ID3D12Resource> indexResourceSprite;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	uint32_t* indexDataSprite = nullptr;
};

