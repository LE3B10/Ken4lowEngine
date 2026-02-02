#pragma once
#include "DX12Include.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Camera.h"
#include "AABB.h"
#include "OBB.h"
#include "Capsule.h"

#include <vector>
#include <list>
#include <map>
#include <Segment.h>
#include <Plane.h>


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
		VertexData* vertexData = nullptr;			 // 頂点データ
		ComPtr<ID3D12Resource> vertexBuffer;		 // 頂点バッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{}; // 頂点バッファビュー
	};

	// 矩形の構造体
	struct BoxData
	{
		VertexData* vertexData = nullptr;		   // 頂点データ
		uint32_t* indexData = nullptr;			   // インデックスデータ
		ComPtr<ID3D12Resource> vertexBuffer;	   // 頂点バッファ
		ComPtr<ID3D12Resource> indexBuffer;		   // インデックスバッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // 頂点バッファビュー
		D3D12_INDEX_BUFFER_VIEW indexBufferView;   // インデックスバッファビュー
	};

	// 線分の構造体
	struct LineData
	{
		VertexData* vertexData = nullptr;		     // 頂点データ
		ComPtr<ID3D12Resource> vertexBuffer;	     // 頂点バッファ
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{}; // 頂点バッファビュー
	};

	// 球体
	struct Sphere
	{
		Vector3 center;
		float radius;
	};

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// Wireframe のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>Wireframe の唯一のインスタンス。</returns>
	static Wireframe* GetInstance();

	/// <summary>
	/// ワイヤーフレーム描画に必要な各種リソースを初期化します。<br/>
	/// ルートシグネチャ／パイプラインステート／頂点バッファ／座標変換バッファなどを生成します。
	/// </summary>
	/// <param name="dxCommon">DirectXCommon へのポインタ。</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// ワイヤーフレーム描画に使用した各種リソースを解放します。<br/>
	/// ルートシグネチャ／パイプラインステート／頂点バッファ／座標変換バッファなどを解放します。
	/// </summary>
	void Finalize();

	/// <summary>
	/// フレームごとの更新処理を行います。<br/>
	/// 主にカメラ情報からビュー・プロジェクション行列を更新します。
	/// </summary>
	void Update();

	/// <summary>
	/// 登録されているワイヤーフレーム図形を全て描画します。<br/>
	/// 事前に DrawLine / DrawSphere などで貯めた頂点を、三角形・線分として描画します。
	/// </summary>
	void Draw();

	/// <summary>
	/// 1 フレーム分の描画が完了した後に、登録済みの図形情報をリセットします。<br/>
	/// 次フレームに向けてインデックス／カウントをクリアします。
	/// </summary>
	void Reset();

public: /// ---------- 2D用の線の描画 ---------- ///

	/// <summary>
	/// 2 点を結ぶ単純な線分を描画します。
	/// </summary>
	/// <param name="start">線分の始点のワールド座標。</param>
	/// <param name="end">線分の終点のワールド座標。</param>
	/// <param name="color">RGBA 形式の色。</param>
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color);

	/// <summary>
	/// Segment 構造体で指定された線分を描画します。
	/// </summary>
	/// <param name="segment">始点と方向(長さ)を持つ線分構造体。</param>
	/// <param name="color">線分の色（RGBA）。</param>
	void DrawSegment(const Segment& segment, const Vector4& color);

	/// <summary>
	/// 3D 空間上に円を描画します（カメラからは円に見える線分の集合）。
	/// </summary>
	/// <param name="center">円の中心位置。</param>
	/// <param name="radius">円の半径。</param>
	/// <param name="segmentCount">円周を分割する線分数。</param>
	/// <param name="color">円の線色（RGBA）。</param>
	void DrawCircle(const Vector3& center, float radius, uint32_t segmentCount, const Vector4& color);

	/// <summary>
	/// 3 点を結んだ三角形をワイヤーで描画します。
	/// </summary>
	/// <param name="position1">1つ目の頂点位置。</param>
	/// <param name="position2">2つ目の頂点位置。</param>
	/// <param name="position3">3つ目の頂点位置。</param>
	/// <param name="color">三角形の線色（RGBA）。</param>
	void DrawTriangle(const Vector3& position1, const Vector3& position2, const Vector3& position3, const Vector4& color);

	/// <summary>
	/// 軸に沿った簡易な四角形（矩形）をワイヤーで描画します。
	/// </summary>
	/// <param name="position">矩形の左下（または基準）位置。</param>
	/// <param name="size">矩形の幅・高さを表すサイズベクトル。</param>
	/// <param name="color">矩形の線色（RGBA）。</param>
	void DrawBox(const Vector3& position, const Vector3& size, const Vector4& color);

	/// <summary>
	/// 五芒星（星型の五角形）を描画します。
	/// </summary>
	/// <param name="center">五芒星の中心位置。</param>
	/// <param name="radius">外接円の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPentagram(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 六芒星（ダビデの星）を描画します。
	/// </summary>
	/// <param name="center">六芒星の中心位置。</param>
	/// <param name="radius">外接円の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawHexagram(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 五芒星 + 外周円 のシンプルな魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣全体のおおよその半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawMagicPentagram(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 任意の辺数を持つ正多角形を描画します。
	/// </summary>
	/// <param name="center">多角形の中心位置。</param>
	/// <param name="radius">外接円の半径。</param>
	/// <param name="sides">辺の数（3以上）。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPolygon(const Vector3& center, float radius, uint32_t sides, const Vector4& color);

	/// <summary>
	/// 魔法陣などでよく使う「同心円」を複数本描画します。
	/// </summary>
	/// <param name="center">同心円の中心位置。</param>
	/// <param name="radius">一番内側の円の半径。</param>
	/// <param name="count">描画する円の本数。</param>
	/// <param name="spacing">円同士の半径差。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawConcentricCircles(const Vector3& center, float radius, uint32_t count, float spacing, const Vector4& color);

	/// <summary>
	/// 円 + 多角形 + 五芒星などを組み合わせた、少しリッチな魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣全体のおおよその半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawAdvancedMagicCircle(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 時間パラメータを使って回転アニメーションする五芒星を描画します。
	/// </summary>
	/// <param name="center">五芒星の中心位置。</param>
	/// <param name="radius">五芒星の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	/// <param name="time">回転量計算に使う時間パラメータ。</param>
	void DrawRotatingPentagram(const Vector3& center, float radius, const Vector4& color, float time);

	/// <summary>
	/// 波紋のように外側へ広がっていく魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="baseRadius">基準となる半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	/// <param name="time">拡大率の変化に使う時間パラメータ。</param>
	void DrawExpandingMagicCircle(const Vector3& center, float baseRadius, const Vector4& color, float time);

	/// <summary>
	/// フェードイン／フェードアウトする「光の輪」風の魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣の半径。</param>
	/// <param name="time">アルファ値の変化に使う時間パラメータ。</param>
	void DrawFadingMagicCircle(const Vector3& center, float radius, float time);

	/// <summary>
	/// 回転 + 拡縮 + フェードなど複数のアニメーションを組み合わせた魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣の基準半径。</param>
	/// <param name="time">各種アニメーションの時間パラメータ。</param>
	void DrawAnimatedMagicCircle(const Vector3& center, float radius, float time);

	/// <summary>
	/// 拡縮などのアニメーション付きハート形を描画します。
	/// </summary>
	/// <param name="center">ハートの中心位置。</param>
	/// <param name="size">ハートの大きさ。</param>
	/// <param name="time">アニメーション用の時間パラメータ。</param>
	void DrawAnimatedHeart(const Vector3& center, float size, float time);

	/// <summary>
	/// 発光しているようなハート形を描画します。
	/// </summary>
	/// <param name="center">ハートの中心位置。</param>
	/// <param name="size">ハートの大きさ。</param>
	/// <param name="time">発光アニメーション用の時間パラメータ。</param>
	void DrawGlowingHeart(const Vector3& center, float size, float time);

	/// <summary>
	/// ポップするような拡縮をするハート形を描画します。
	/// </summary>
	/// <param name="center">ハートの中心位置。</param>
	/// <param name="size">ハートの基本サイズ。</param>
	/// <param name="time">拡大・縮小に使う時間パラメータ。</param>
	void DrawPoppingHeart(const Vector3& center, float size, float time);

	/// <summary>
	/// ふわっと浮上するハート形を描画します。
	/// </summary>
	/// <param name="basePosition">ハートの基準位置（浮上前）。</param>
	/// <param name="size">ハートの大きさ。</param>
	/// <param name="time">上下移動などに使う時間パラメータ。</param>
	void DrawFloatingHeart(const Vector3& basePosition, float size, float time);

	/// <summary>
	/// シンプルな魔法陣（円や十字など）を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawMagicCircle(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 回転アニメーション付きの魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawRotatingMagicCircle(const Vector3& center, float radius, const Vector4& color);

	/// <summary>
	/// 描画進行度に応じて徐々に描かれていく魔法陣を描画します。
	/// </summary>
	/// <param name="center">魔法陣の中心位置。</param>
	/// <param name="radius">魔法陣の半径。</param>
	/// <param name="baseColor">ベースカラー（アルファ含む）。</param>
	/// <param name="time">進行度に使う時間パラメータ。</param>
	void DrawProgressiveMagicCircle(const Vector3& center, float radius, const Vector4& baseColor, float time);

	/// <summary>
	/// 五芒星を 0.0～1.0 の進行度に応じて描画する関数です。
	/// </summary>
	/// <param name="center">五芒星の中心位置。</param>
	/// <param name="radius">五芒星の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	/// <param name="reloadProgress">0.0～1.0 の描画進行度。</param>
	void DrawPentagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);

	/// <summary>
	/// 六芒星を 0.0～1.0 の進行度に応じて描画する関数です。
	/// </summary>
	/// <param name="center">六芒星の中心位置。</param>
	/// <param name="radius">六芒星の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	/// <param name="reloadProgress">0.0～1.0 の描画進行度。</param>
	void DrawHexagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress);

	/// <summary>
	/// 3D 空間にグリッド（方眼）を描画します。<br/>
	/// 地面の目安やデバッグ用の床として利用できます。
	/// </summary>
	/// <param name="size">グリッド全体のサイズ（片側の長さ）。</param>
	/// <param name="subdivision">1マスあたりの長さ。</param>
	/// <param name="color">グリッド線の色（RGBA）。</param>
	void DrawGrid(const float size, const float subdivision, const Vector4& color);

public: /// ---------- 3D用の線の描画 ---------- ///

	/// <summary>
	/// AABB（軸平行境界ボックス）をワイヤーフレームで描画します。
	/// </summary>
	/// <param name="aabb">最小・最大座標を持つ AABB 構造体。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawAABB(const AABB& aabb, const Vector4& color);

	/// <summary>
	/// OBB（任意回転可能な境界ボックス）をワイヤーフレームで描画します。
	/// </summary>
	/// <param name="obb">中心・半サイズ・回転軸を持つ OBB 構造体。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawOBB(const OBB& obb, const Vector4& color);

	/// <summary>
	/// 球体を緯度経度線のような線分で描画します。
	/// </summary>
	/// <param name="center">球の中心位置。</param>
	/// <param name="radius">球の半径。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawSphere(const Vector3& center, const float radius, const Vector4& color);

	/// <summary>
	/// 中心 + 高さ + 半径 + 軸ベクトルを指定してカプセルを描画します。
	/// </summary>
	/// <param name="center">カプセルの中心位置。</param>
	/// <param name="radius">カプセルの半径。</param>
	/// <param name="height">シリンダー部分を含めた全体の高さ。</param>
	/// <param name="axis">高さ方向を表す軸ベクトル。</param>
	/// <param name="segments">分割数（輪切り数）。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawCapsule(const Vector3& center, float radius, float height, const Vector3& axis, uint32_t segments, const Vector4& color);

	/// <summary>
	/// Capsule 構造体からカプセルコライダをワイヤーフレームで描画します。
	/// </summary>
	/// <param name="capsule">線分と半径を持つカプセル構造体。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawCapsule(const Capsule& capsule, const Vector4& color);

	/// <summary>
	/// Plane 構造体から平面を一定サイズの四角形として描画します。
	/// </summary>
	/// <param name="plane">法線と距離を持つ平面構造体。</param>
	/// <param name="size">描画する平面の一辺の長さ。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPlane(const Plane& plane, float size, const Vector4& color);

	/// <summary>
	/// 円柱（シリンダー）をワイヤーフレームで描画します。
	/// </summary>
	/// <param name="baseCenter">円柱の底面中心位置。</param>
	/// <param name="radius">底面の半径。</param>
	/// <param name="height">円柱の高さ。</param>
	/// <param name="axis">高さ方向の軸ベクトル。</param>
	/// <param name="segmentCount">円周の分割数。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawCylinder(const Vector3& baseCenter, float radius, float height, const Vector3& axis, uint32_t segmentCount, const Vector4& color);

	/// <summary>
	/// 三角錐（テトラヘドロン）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="baseCenter">底面の中心位置。</param>
	/// <param name="baseSize">底面三角形の一辺の長さ。</param>
	/// <param name="height">ピラミッドの高さ。</param>
	/// <param name="axis">高さ方向の軸ベクトル。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawTetrahedron(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);

	/// <summary>
	/// 四角錐（ピラミッド）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="baseCenter">底面の中心位置。</param>
	/// <param name="baseSize">底面正方形の一辺の長さ。</param>
	/// <param name="height">ピラミッドの高さ。</param>
	/// <param name="axis">高さ方向の軸ベクトル。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPyramid(const Vector3& baseCenter, float baseSize, float height, const Vector3& axis, const Vector4& color);

	/// <summary>
	/// 正八面体（オクタヘドロン）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="center">正八面体の中心位置。</param>
	/// <param name="size">全体のサイズ（対極頂点間の距離などの基準）。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawOctahedron(const Vector3& center, float size, const Vector4& color);

	/// <summary>
	/// 正十二面体（ドデカヘドロン）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="center">正十二面体の中心位置。</param>
	/// <param name="size">全体のサイズ（スケール）。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawDodecahedron(const Vector3& center, float size, const Vector4& color);

	/// <summary>
	/// 正二十面体（イコサヘドロン）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="center">正二十面体の中心位置。</param>
	/// <param name="size">全体のサイズ（スケール）。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawIcosahedron(const Vector3& center, float size, const Vector4& color);

	/// <summary>
	/// トーラス（ドーナツ形状）をワイヤーフレームで描画します。
	/// </summary>
	/// <param name="center">トーラスの中心位置。</param>
	/// <param name="R">大きい半径（ドーナツの中心から管の中心まで）。</param>
	/// <param name="r">小さい半径（管の太さ）。</param>
	/// <param name="ringSegments">大円方向の分割数。</param>
	/// <param name="tubeSegments">小円方向の分割数。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);

	/// <summary>
	/// 回転アニメーション付きのトーラスを描画します。
	/// </summary>
	/// <param name="center">トーラスの中心位置。</param>
	/// <param name="R">大きい半径。</param>
	/// <param name="r">小さい半径。</param>
	/// <param name="ringSegments">大円方向の分割数。</param>
	/// <param name="tubeSegments">小円方向の分割数。</param>
	/// <param name="color">線色（RGBA）。</param>
	/// <param name="time">回転などに使う時間パラメータ。</param>
	void DrawRotatingTorus(const Vector3& center, float R, float r, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color, float time);

	/// <summary>
	/// メビウスの帯（ねじれた輪）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="center">メビウスの帯の中心位置。</param>
	/// <param name="R">帯の中心線となる円の半径。</param>
	/// <param name="w">帯の幅。</param>
	/// <param name="ringSegments">輪の分割数。</param>
	/// <param name="tubeSegments">幅方向の分割数。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawMobiusStrip(const Vector3& center, float R, float w, uint32_t ringSegments, uint32_t tubeSegments, const Vector4& color);

	/// <summary>
	/// 8 の字のような立体レムニスケート曲線を描画します。
	/// </summary>
	/// <param name="center">レムニスケートの中心位置。</param>
	/// <param name="a">x 成分のスケール。</param>
	/// <param name="b">y 成分のスケール。</param>
	/// <param name="c">z 成分のスケール。</param>
	/// <param name="segments">分割数。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawLemniscate3D(const Vector3& center, float a, float b, float c, uint32_t segments, const Vector4& color);

public: /// ---------- 五角形 ---------- ///

	/// <summary>
	/// 五角柱（底面が五角形の柱）をワイヤーフレーム描画します。
	/// </summary>
	/// <param name="center">五角柱の中心位置。</param>
	/// <param name="radius">底面五角形の外接円半径。</param>
	/// <param name="height">柱の高さ。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPentagonalPrism(const Vector3& center, float radius, float height, const Vector4& color);

	/// <summary>
	/// 五角錐（底面が五角形のピラミッド）を描画します。
	/// </summary>
	/// <param name="center">五角錐の底面中心位置。</param>
	/// <param name="radius">底面五角形の外接円半径。</param>
	/// <param name="height">五角錐の高さ。</param>
	/// <param name="color">線色（RGBA）。</param>
	void DrawPentagonalPyramid(const Vector3& center, float radius, float height, const Vector4& color);

public: /// ---------- 設定 ---------- ///

	/// <summary>
	/// ワイヤーフレーム描画に使用するカメラを設定します。
	/// </summary>
	/// <param name="camera">ビュー行列・プロジェクションを取得するカメラポインタ。</param>
	void SetCamera(Camera* camera) { camera_ = camera; }

	/// <summary>
	/// 使用するプロジェクション行列を直接設定します。
	/// </summary>
	/// <param name="projectionMatrix">使用したいプロジェクション行列。</param>
	void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }

	/// <summary>
	/// デバッグカメラを使用するかどうかを設定します。
	/// </summary>
	/// <param name="isDebugCamera">デバッグカメラ使用時 true。</param>
	void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

public: /// ---------- 取得 ---------- ///

	/// <summary>
	/// 現在使用中のプロジェクション行列を取得します。
	/// </summary>
	/// <returns>プロジェクション行列。</returns>
	const Matrix4x4& GetProjectionMatrix() { return projectionMatrix_; }

	/// <summary>
	/// デバッグカメラを使用しているかどうかを取得します。
	/// </summary>
	/// <returns>デバッグカメラ使用時 true。</returns>
	bool GetDebugCamera() const { return isDebugCamera_; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ワイヤーフレーム描画用のルートシグネチャを生成します。<br/>
	/// 三角形描画用／線分描画用などで共通のレイアウトを作成し、<br/>
	/// 引数の ComPtr に作成結果を格納します。
	/// </summary>
	/// <param name="rootSignature">
	/// 生成した ID3D12RootSignature を格納する出力用の ComPtr 参照。
	/// </param>
	void CreateRootSignature(ComPtr<ID3D12RootSignature>& rootSignature);

	/// <summary>
	/// 指定したプリミティブトポロジー用のグラフィックスパイプラインステート(PSO)を生成します。<br/>
	/// ・入力レイアウト<br/>
	/// ・ルートシグネチャ<br/>
	/// ・ブレンド／ラスタライザ／深度ステンシル設定<br/>
	/// ・VS / PS シェーダ<br/>
	/// などを設定し、ID3D12PipelineState を作成して返します。
	/// </summary>
	/// <param name="primitiveTopologyType">
	/// 使用するプリミティブトポロジー種別（線分／三角形など）。
	/// </param>
	/// <param name="rootSignature">
	/// 使用するルートシグネチャ。事前に CreateRootSignature() で生成されていることを想定。
	/// </param>
	/// <param name="pipelineState">
	/// 生成した ID3D12PipelineState を格納する出力用の ComPtr 参照。
	/// </param>
	void CreatePSO(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType,
		ComPtr<ID3D12RootSignature>& rootSignature,
		ComPtr<ID3D12PipelineState>& pipelineState);

	/// <summary>
	/// 三角形描画用の頂点バッファを生成します。<br/>
	/// ・最大三角形数(kTriangleMaxCount)×3 頂点分のバッファを確保<br/>
	/// ・頂点バッファを GPU リソースとして生成＆マップ<br/>
	/// ・VertexData へのポインタ／VBV を TriangleData に設定<br/>
	/// といった初期化を行います。
	/// </summary>
	/// <param name="triangleData">
	/// 三角形用頂点データ／バッファ情報を保持する構造体へのポインタ。
	/// </param>
	void CreateTriangleVertexData(TriangleData* triangleData);

	/// <summary>
	/// 矩形描画用の頂点バッファ・インデックスバッファを生成します。<br/>
	/// ・最大矩形数(kBoxMaxCount)分の頂点／インデックスバッファを確保<br/>
	/// ・GPU リソースの作成＆マップ<br/>
	/// ・VertexData / index 配列へのポインタと VBV / IBV を BoxData に設定<br/>
	/// といった初期化を行います。
	/// </summary>
	/// <param name="boxData">
	/// 矩形用頂点／インデックスバッファ情報を保持する構造体へのポインタ。
	/// </param>
	void CreateBoxVertexData(BoxData* boxData);

	/// <summary>
	/// 線分描画用の頂点バッファを生成します。<br/>
	/// ・最大線分数(kLineMaxCount_)×2 頂点分のバッファを確保<br/>
	/// ・GPU リソースを生成してマップし、VertexData へのポインタを保存<br/>
	/// ・頂点バッファビュー(VBV)を設定します。
	/// </summary>
	/// <param name="lineData">
	/// 線分用頂点データ／バッファ情報を保持する構造体へのポインタ。
	/// </param>
	void CreateLineVertexData(LineData* lineData);

	/// <summary>
	/// 座標変換用の定数バッファ(ConstantBuffer)を生成します。<br/>
	/// ・TransformationMatrix( WVP 行列 ) 用のバッファを確保<br/>
	/// ・バッファをマップして transformationMatrixData_ にアドレスを保持<br/>
	/// することで、毎フレームのビュー・プロジェクション更新に対応します。
	/// </summary>
	void CreateTransformationMatrix();

	/// <summary>
	/// 球体描画用の頂点（緯度・経度ライン）を事前計算して格納します。<br/>
	/// Wireframe の球描画で再利用するため、spheres_ に一度だけ座標列を構築します。
	/// </summary>
	void CalcSphereVertexData();

private: /// ---------- メンバ変数 ---------- ///

	// DirectXCommon
	DirectXCommon* dxCommon_ = nullptr;
	// カメラ
	Camera* camera_ = nullptr;

	BlendMode blendMode_ = BlendMode::kBlendModeNormal;

	// ルートシグネチャ
	ComPtr<ID3D12RootSignature> triangleRootSignature_;
	ComPtr<ID3D12RootSignature> lineRootSignature_;

	// パイプラインステート
	ComPtr<ID3D12PipelineState> trianglePipelineState_;
	ComPtr<ID3D12PipelineState> linePipelineState_;

	// 座標変換行列バッファ
	ComPtr<ID3D12Resource> transformationMatrixBuffer_;

	// 座標変換行列データ
	TransformationMatrix* transformationMatrixData_ = nullptr;

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
	bool isDebugCamera_ = false;

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
	const uint32_t kLineMaxCount_ = 1000000;
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
