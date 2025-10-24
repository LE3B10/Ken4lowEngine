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
///						スプライトクラス
/// -------------------------------------------------------------
class Sprite
{
private: /// ---------- 定数 ---------- ///

	static inline const UINT kNumVertex = 6; // 四角形を描画するための頂点数
	static inline const UINT kNumIndex = 4;  // 四角形を描画するためのインデックス数

private: /// ---------- 構造体　----------- ///

	// マテリアルデータの構造体
	struct Material final
	{
		Vector4 color;		   // 色(RGBA)
		Matrix4x4 uvTransform; // UV変換行列
		float padding[3];	   // パディング
	};

	// 頂点データの構造体
	struct VertexData
	{
		Vector4 position; // 座標
		Vector2 texcoord; // テクスチャ座標
	};

	// 座標変換行列データの構造体
	struct TransformationMatrix final
	{
		Matrix4x4 WVP;	 // ワールドビュー射影変換行列
		Matrix4x4 World; // ワールド変換行列
	};

	// リロード進捗の構造体
	struct ReloadProgress
	{
		bool isReloading = false; // リロード中かどうか
		float progress = 0.0f;	  // 進捗度合い(0.0f〜1.0f)
		float padding[2];		  // パディング
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// スプライトを初期化する
	/// </summary>
	/// <param name="filePath">テクスチャファイルパス</param>
	void Initialize(const std::string& filePath);

	/// <summary>
	/// スプライトの更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// スプライトの描画処理
	/// </summary>
	void Draw();

public: /// ---------- ゲッター ---------- ///

	// 左右フリップを取得
	bool GetFlipX() const { return isFlipX_; }

	// 上下フリップを取得
	bool GetFlipY() const { return isFlipY_; }

	// 座標を取得
	const Vector2& GetPosition() const { return position_; }

	// 回転を取得
	float GetRotation() const { return rotation_; }

	// サイズを取得
	const Vector2& GetSize() const { return size_; }

	// 色を取得
	const Vector4& GetColor() const { return materialData->color; }

	// アンカーを取得
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	// テクスチャ左上座標を取得
	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

	// テクスチャ切り出しサイズを取得
	const Vector2& GetTextureSize() { return textureSize_; }

public: /// ---------- セッター ---------- ///

	// 左右フリップの設定
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	// 上下フリップの設定
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

	// 座標の設定
	void SetPosition(const Vector2& position) { position_ = position; }

	// 回転の設定
	void SetRotation(float rotation) { rotation_ = rotation; }

	// サイズの設定
	void SetSize(const Vector2& size) { size_ = size; }

	// 色の設定
	void SetColor(const Vector4& color) { materialData->color = color; }

	// アンカーの設定
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	// テクスチャ左上座標の設定
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	// テクスチャ切り出しサイズの設定
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

	// テクスチャの変更
	void SetTexture(const std::string& filePath);

	// UV座標の設定
	void SetUVRect(const Vector2& leftTop, const Vector2& size) { textureLeftTop_ = leftTop; textureSize_ = size; }

	// リロード進捗の設定
	void SetReloadProgress(bool isReloading, float progress)
	{
		reloadProgressData->isReloading = isReloading; // リロード中かどうか
		reloadProgressData->progress = progress;	   // 進捗度合い(0.0f〜1.0f)
	}

private: /// ---------- メンバ関数 ---------- ///

	// スプライト用のマテリアルリソースを作成し設定する処理を行う
	void CreateMaterialResource();

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	void CreateVertexBufferResource();

	// スプライトのインデックスバッファを作成
	void CreateIndexBuffer();

	// テクスチャ債ぞをイメージに合わせる
	void AdjustTextureSize();

	// リロード新緑の初期化処理
	void InitializeReloadProgress();

private: /// ---------- メンバ変数 ---------- ///

	// 左右フリップ
	bool isFlipX_ = false;

	// 上下フリップ
	bool isFlipY_ = false;

	// 座標
	Vector2 position_ = { 0.0f, 0.0f };
	// 回転
	float rotation_ = 0;
	// サイズ
	Vector2 size_ = { 1.0f, 1.0f };
	//アンカーポイント
	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.0f, 100.0f };
	// 色
	Vector4 color_ = {};

	// テクスチャ番号
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};

	std::string filePath_;

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	//スプライト用のマテリアルソースを作る
	ComPtr <ID3D12Resource> materialResource;
	Material* materialData = nullptr;

	// スプライトの頂点バッファリソースと変換行列リソースを生成
	//Sprite用の頂点リソースを作る
	ComPtr <ID3D12Resource> vertexResource;
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// 頂点データを設定する
	VertexData* vertexData = nullptr;
	//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	ComPtr <ID3D12Resource> transformationMatrixResource;
	//データを書き込む
	TransformationMatrix* transformationMatrixData = nullptr;

	// スプライトのインデックスバッファを作成および設定する
	ComPtr <ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t* indexData = nullptr;

	// リロード進捗のリソース
	ComPtr <ID3D12Resource> reloadProgressResource;
	ReloadProgress* reloadProgressData = nullptr;
};

