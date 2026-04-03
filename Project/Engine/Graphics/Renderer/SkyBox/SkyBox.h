#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "Camera.h"

#include "Vector2.h"
#include "Vector4.h"

#include <array>
#include <memory>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///					　スカイボックスクラス
/// -------------------------------------------------------------
class SkyBox
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- 頂点数 ( Vertex, Index ) ----------- ///
	static inline const UINT kNumVertex = 36;
	static inline const UINT kNumIndex = 36;

	// マテリアルデータの構造体
	struct Material final
	{
		Vector4 color;
		Matrix4x4 uvTransform;
		uint32_t textureIndex;
		float padding[3];
	};

	// 頂点データの構造体
	struct VertexData
	{
		Vector4 position;
		Vector3 texcoord;
	};

	// 座標変換行列データの構造体
	struct TransformationMatrix final
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// スカイボックスの初期化処理を行います。<br/>
	/// ・テクスチャの読み込みと SRV ハンドル取得<br/>
	/// ・デフォルトカメラ / DirectXCommon の取得<br/>
	/// ・スケール / 回転 / 平行移動の初期化<br/>
	/// ・マテリアル / 頂点 / インデックス / WVP バッファの生成<br/>
	/// などをまとめて行います。
	/// </summary>
	/// <param name="filePath">読み込む環境テクスチャのファイルパス。</param>
	void Initialize(const std::string& filePath);

	/// <summary>
	/// 毎フレームの更新処理を行います。<br/>
	/// ・ワールド行列の計算（スケール / 回転 / 平行移動）<br/>
	/// ・カメラ or デバッグカメラのビュー・プロジェクション行列取得<br/>
	/// ・World * ViewProjection を計算して WVP を更新<br/>
	/// ・カメラに ViewProjection 行列をセット<br/>
	/// を行い、描画用の定数バッファを書き換えます。
	/// </summary>
	void Update();

	/// <summary>
	/// スカイボックスの描画処理を行います。<br/>
	/// ・SkyBoxManager で PSO / ルートシグネチャをセット<br/>
	/// ・頂点 / インデックスバッファのバインド<br/>
	/// ・マテリアル / WVP 定数バッファのバインド<br/>
	/// ・環境テクスチャの SRV 設定<br/>
	/// ・DrawIndexedInstanced によるキューブ描画<br/>
	/// を行います。
	/// </summary>
	void Draw();

	/// <summary>
	/// デバッグカメラを使用するかどうかを設定します。<br/>
	/// true の場合、Update 時に DebugCamera の ViewProjection を使用します。
	/// </summary>
	/// <param name="isDebugCamera">デバッグカメラを使用する場合は true。</param>
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

	/// <summary>
	/// デバッグカメラを使用しているかどうかを取得します。
	/// </summary>
	/// <returns>デバッグカメラ使用時 true。</returns>
	bool GetDebugCamera() { return isDebugCamera_; }

	/// <summary>
	/// 環境マップとして使用しているテクスチャの GPU ディスクリプタハンドルを取得します。<br/>
	/// IBL など、他の描画パスで利用したい場合に参照します。
	/// </summary>
	/// <returns>SRV の GPU ディスクリプタハンドル。</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapHandle() const { return gpuHandle_; }

public: /// ---------- アクセッサ ---------- ///

	// 座標を取得 : 設定
	WorldTransform& GetWorldTransform() { return worldTransform_; }
	void SetWorldTransform(const WorldTransform& worldTransform) { worldTransform_ = worldTransform; }

private: /// ---------- 内部メンバ関数 ---------- ///

	/// <summary>
	/// マテリアル定数バッファを初期化します。<br/>
	/// ・Material 用のバッファリソースを生成<br/>
	/// ・マップして書き込み用ポインタを取得<br/>
	/// ・色を白(1,1,1,1)、UV 行列を単位行列に設定<br/>
	/// を行います。
	/// </summary>
	void InitializeMaterial();

	/// <summary>
	/// キューブ形状の頂点バッファを初期化します。<br/>
	/// ・VertexData × kNumVertex 分のバッファリソースを生成<br/>
	/// ・頂点バッファビューを設定<br/>
	/// ・左右 / 前後 / 上下 6 面分の頂点位置 & テクスチャ座標をセット<br/>
	/// を行います。
	/// </summary>
	void InitializeVertexBufferData();

	/// <summary>
	/// キューブ形状のインデックスバッファを初期化します。<br/>
	/// ・uint32_t × kNumIndex 分のバッファリソースを生成<br/>
	/// ・インデックスバッファビューを設定<br/>
	/// ・各面 2 三角形分のインデックスを時計回りで登録<br/>
	/// を行います。
	/// </summary>
	void InitializeIndexData();

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	Camera* camera_ = nullptr;

	// ワールド行列の計算
	WorldTransform worldTransform_;

	// テクスチャ番号
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_ = {};

	//スプライト用のマテリアルソースを作る
	ComPtr <ID3D12Resource> materialResource;
	Material* materialData_ = nullptr;

	ComPtr <ID3D12Resource> vertexResource;// 頂点リソースを作る
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};// 頂点バッファビューを作成する
	VertexData* vertexData_ = nullptr;// 頂点データを設定する
	ComPtr <ID3D12Resource> wvpResource;// TransformationMatrix用のリソース
	TransformationMatrix* wvpData = nullptr;//データを書き込む

	// インデックスバッファを作成および設定する
	ComPtr <ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	uint32_t* indexData_ = nullptr;

	Matrix4x4 worldViewProjectionMatrix;
	Matrix4x4 viewProjectionMatrix_;
	Matrix4x4 debugViewProjectionMatrix_;
	bool isDebugCamera_ = false;

	// テクスチャインデックス
	uint32_t textureIndex_ = 0;
};


} // namespace Ken4lowEngine
