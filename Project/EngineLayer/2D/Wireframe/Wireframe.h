#pragma once
#include "DX12Include.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Camera.h"
#include "AABB.h"
#include "OBB.h"

#include <vector>
#include <list>
#include <map>


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　	線で形状を描画するクラス
/// -------------------------------------------------------------
class Wireframe
{
public: /// ---------- テンプレート ---------- ///

	// ComPtrのエイリアス
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: /// ---------- 構造体 ---------- ///

	// 頂点データ
	struct VertexData
	{
		Vector3 position; // 座標
		Vector4 color;	  // 色
	};

	// 座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
	};

	// 三角形の構造体
	struct TriangleData
	{
		VertexData* vertexData;					   // 頂点データ
		ComPtr<ID3D12Resource> vertexBuffer;	   // 頂点バッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // 頂点バッファビュー
	};

	// 矩形の構造体
	struct BoxData
	{
		VertexData* vertexData;					   // 頂点データ
		uint32_t* indexData;					   // インデックスデータ
		ComPtr<ID3D12Resource> vertexBuffer;	   // 頂点バッファ
		ComPtr<ID3D12Resource> indexBuffer;		   // インデックスバッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // 頂点バッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView;   // インデックスバッファビュー
	};

	// 線分の構造体
	struct LineData
	{
		VertexData* vertexData;					   // 頂点データ
		ComPtr<ID3D12Resource> vertexBuffer;	   // 頂点バッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // 頂点バッファビュー
	};

	// 球体
	struct Sphere
	{
		Vector3 center;
		float radius;
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static Wireframe* GetInstance();

	// 初期化処理
	void Initialize(DirectXCommon* dxCommon);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// リセット処理
	void Reset();

public: /// ---------- 2D用の線の描画 ---------- ///

	// 線を描画
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

	// 円を描画
	void DrawCircle(const Vector3& center, float radius, uint32_t segmentCount, const Vector4& color);

	// 三角形を描画
	void DrawTriangle(const Vector3& position1, const Vector3& position2, const Vector3& position3, const Vector4& color);

	// 四角形を描画
	void DrawBox(const Vector3& position, const Vector3& size, const Vector4& color);

	// 五芒星（ペンタグラム）を描画
	void DrawPentagram(const Vector3& center, float radius, const Vector4& color);

	// 六芒星（ヘキサグラム・ダビデの星）
	void DrawHexagram(const Vector3& center, float radius, const Vector4& color);

	// 星型魔法陣（外円付きの五芒星）五芒星に円を追加すると、魔法陣らしくなる
	void DrawMagicPentagram(const Vector3& center, float radius, const Vector4& color);

	// 正多角形（ポリゴン）任意のn角形を描画できる汎用関数
	void DrawPolygon(const Vector3& center, float radius, uint32_t sides, const Vector4& color);

	// 同心円（Concentric Circles）魔法陣やエフェクト表現でよく使う円の重なり。
	void DrawConcentricCircles(const Vector3& center, float radius, uint32_t count, float spacing, const Vector4& color);

	// 組み合わせて魔法陣を作成
	void DrawAdvancedMagicCircle(const Vector3& center, float radius, const Vector4& color);

	// 回転する魔法陣
	void DrawRotatingPentagram(const Vector3& center, float radius, const Vector4& color, float time);

	// 波紋のように広がる魔法陣
	void DrawExpandingMagicCircle(const Vector3& center, float baseRadius, const Vector4& color, float time);

	// 光の輪（フェードイン・フェードアウト）
	void DrawFadingMagicCircle(const Vector3& center, float radius, float time);

	// 回転 + 拡縮 + フェードイン・アウトを合体した魔法陣
	void DrawAnimatedMagicCircle(const Vector3& center, float radius, float time);

	// ハート形（遊び心）
	void DrawAnimatedHeart(const Vector3& center, float size, float time);

	// ハートが光エフェクト
	void DrawGlowingHeart(const Vector3& center, float size, float time);

	// ハートがポップする
	void DrawPoppingHeart(const Vector3& center, float size, float time);

	// ハートが浮上する
	void DrawFloatingHeart(const Vector3& basePosition, float size, float time);

	// 魔法陣
	void DrawMagicCircle(const Vector3& center, float radius, const Vector4& color);

	// 回転する魔法陣
	void DrawRotatingMagicCircle(const Vector3& center, float radius, const Vector4& color, float time);

	void DrawProgressiveMagicCircle(const Vector3& center, float radius, const Vector4& baseColor, float time);

	void DrawPentagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);

	void DrawHexagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);

	// グリッドを描画
	void DrawGrid(const float size, const float subdivision, const Vector4& color);

public: /// ---------- 3D用の線の描画 ---------- ///

	// AABBを描画
	void DrawAABB(const AABB& aabb, const Vector4& color);

	// OBBを描画
	void DrawOBB(const OBB& obb, const Vector4& color);

	// 球体を描画
	void DrawSphere(const Vector3& center, const float radius, const Vector4& color);

	// 円柱（シリンダー）を描画
	void DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color);

	// 三角錐（四面体・テトラへドロン）
	void DrawTetrahedron(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);

	// 四角錐（ピラミッド）を描画
	void DrawPyramid(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);

	// 正八面体（オクタへドロン）
	void DrawOctahedron(const Vector3& center, float size, const Vector4& color);

	// 正十二面体（ドデカへドロン）
	void DrawDodecahedron(const Vector3& center, float size, const Vector4& color);

	// 正二十面体（ICO球・イコサヒードロン）
	void DrawIcosahedron(const Vector3& center, float size, const Vector4& color);

	// トーラス（円環・ドーナツ型）
	void DrawTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);

	// アニメーションのトーラス
	void DrawRotatingTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color, float time);

	// メビウスの帯
	void DrawMobiusStrip(const Vector3& center, float R, float w, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);

	// レムニスケート
	void DrawLemniscate3D(const Vector3& center, float a, float b, float c, uint32_t segments, const Vector4& color);

public: /// ---------- 五角形 ---------- ///

	// 五角柱
	void DrawPentagonalPrism(const Vector3& center, float radius, float height, const Vector4& color);

	// 五角錐（Pentagonal Pyramid）
	void DrawPentagonalPyramid(const Vector3& center, float radius, float height, const Vector4& color);

public: /// ---------- 設定 ---------- ///

	// カメラを設定
	void SetCamera(Camera* camera) { camera_ = camera; }

	// プロジェクションを設定
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	// デバッグカメラの有無
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

public: /// ---------- 取得 ---------- ///

	// プロジェクションを取得
	const Matrix4x4& GetProjectionMatrix() { return projectionMatrix_; }

	// デバッグカメラの有無を取得
	bool GetDebugCamera() { return isDebugCamera_; }

private: /// ---------- メンバ関数 ---------- ///

	// ルートシグネチャの生成
	void CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);

	// PSOの生成
	void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12PipelineState>& pipelineState);

	// 三角形の頂点データを生成
	void CreateTriangleVertexData(TriangleData* triangleData);

	// 矩形の頂点データを生成
	void CreateBoxVertexData(BoxData* boxData);

	// 線の頂点データを生成
	void CreateLineVertexData(LineData* lineData);

	// 座標変換行列データを生成
	void CreateTransformationMatrix();

	// 球の頂点座標を計算
	void CalcSphereVertexData();

private: /// ---------- メンバ変数 ---------- ///

	// DirectXCommon
	DirectXCommon* dxCommon_ = nullptr;
	// カメラ
	Camera* camera_ = nullptr;

	// ルートシグネチャ
	ComPtr<ID3D12RootSignature> triangleRootSignature_;
	ComPtr<ID3D12RootSignature> lineRootSignature_;

	// パイプラインステート
	ComPtr<ID3D12PipelineState> trianglePipelineState_;
	ComPtr<ID3D12PipelineState> linePipelineState_;

	// 座標変換行列バッファ
	ComPtr<ID3D12Resource> transformationMatrixBuffer_;

	// 座標変換行列データ
	TransformationMatrix* transformationMatrixData_;

	// 三角形データ
	std::unique_ptr<TriangleData> triangleData_;

	// 矩形データ
	std::unique_ptr<BoxData> boxData_;

	// 線データ
	std::unique_ptr<LineData> lineData_;

	// 球のデータ
	std::vector<Vector3> spheres_;

private: /// ---------- メンバ変数 ---------- ///

	// デバッグカメラの有無
	bool isDebugCamera_;

	// 三角形
	uint32_t triangleIndex_ = 0; // 三角形のインデックス
	const uint32_t kTriangleMaxCount = 30096;
	const uint32_t kTriangleVertexCount = 3;

	// 矩形
	uint32_t boxIndex_ = 0;
	uint32_t boxVertexIndex_ = 0;
	const uint32_t kBoxMaxCount = 30096;
	const uint32_t kBoxIndexCount = 6;
	const uint32_t kBoxVertexCount = 4;

	// 線分
	uint32_t lineIndex_ = 0;
	const uint32_t kLineMaxCount_ = 100000;
	const uint32_t kLineVertexCount = 2;

	// マトリックス
	Matrix4x4 projectionMatrix_{};
	Matrix4x4 viewProjectionMatrix_{};

	// デバッグ用マトリックス
	Matrix4x4 debugProjectionMatrix_{};
	Matrix4x4 debugViewProjectionMatrix_{};

private: /// ---------- コピー禁止 ---------- ///

	Wireframe() = default;
	~Wireframe() = default;
	Wireframe(const Wireframe&) = delete;
	Wireframe& operator=(const Wireframe&) = delete;
};
