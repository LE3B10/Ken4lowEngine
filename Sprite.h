#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "TextureManager.h"
#include "TransformationMatrix.h"
#include "ResourceManager.h"
#include "VertexData.h"
#include "Transform.h"

#include <array>
#include <memory>

class DirectXCommon;

/// ---------- スプライトの頂点数 ( Vertex, Index ) ----------- ///
static const UINT kNumVertex = 6;
static const UINT kNumIndex = 4;

/// -------------------------------------------------------------
///						スプライトクラス
/// -------------------------------------------------------------
class Sprite
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const std::string& filePath);

	// 更新処理
	void Update();

	// ドローコール
	void DrawCall(ID3D12GraphicsCommandList* commandList);

public: /// ---------- セッター ---------- ///

	// VBV - IBV - CBVの設定（スプライト用）
	void SetSpriteBufferData(ID3D12GraphicsCommandList* commandList);

	//void SetTransform2D(const Transform2D transform2D) { transform2D_ = transform2D; }

private: /// ---------- メンバ関数 ---------- ///

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	void CreateMaterialResource(DirectXCommon* dxCommon);

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	void CreateVertexBufferResource(DirectXCommon* dxCommon);

	// スプライトのインデックスバッファを作成
	void CreateIndexBuffer(DirectXCommon* dxCommon);

	// テクスチャ債ぞをイメージに合わせる
	void AdjustTextureSize();

public: /// ---------- ゲッターとアクセッサ ---------- ///

	// 左右フリップのアクセッサ
	bool GetFlipX() { return isFlipX_; }
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	// 上下フリップのアクセッサ
	bool GetFlipY() { return isFlipY_; }
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }


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
	void SetColor(const Vector4& color) { materialDataSprite->color = color; }

	// アンカーのアクセッサー
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	// テクスチャ左上座標のアクセッサー
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	// テクスチャ切り出しサイズのアクセッサー
	const Vector2& GetTextureSize() { return textureSize_; }
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

private: /// ---------- メンバ変数 ---------- ///

	// 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

	// 座標
	Vector2 position_ = { 0.0f, 0.0f };
	// 回転
	float rotation_;
	// サイズ
	Vector2 size_ = { 1.0f, 1.0f };
	//アンカーポイント
	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.0f, 100.0f };
	// 色
	Vector4 color_;

	// テクスチャ番号
	uint32_t textureIndex = 0;

private: /// ---------- メンバ変数 ---------- ///

	// CreateBuffer用
	ResourceManager* createBuffer_ = nullptr;

	//スプライト用のマテリアルソースを作る
	ComPtr <ID3D12Resource> materialResourceSprite;
	Material* materialDataSprite = nullptr;

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	//Sprite用の頂点リソースを作る
	ComPtr <ID3D12Resource> vertexResourceSprite;
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// 頂点データを設定する
	VertexData* vertexDataSprite = nullptr;
	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	ComPtr <ID3D12Resource> transformationMatrixResourceSprite;
	//データを書き込む
	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	// スプライトのインデックスバッファを作成および設定する
	ComPtr <ID3D12Resource> indexResourceSprite;
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	uint32_t* indexDataSprite = nullptr;
};

