#pragma once
#include <DX12Include.h>
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class Camera;


/// -------------------------------------------------------------
///				　	ワールド変換データクラス
/// -------------------------------------------------------------
class WorldTransform
{
public: /// ---------- 構造体 ---------- ///

	// 座標変換行列データ
	struct TransformationMatrix final
	{
		Matrix4x4 WVP;					  // ワールド・ビュー・プロジェクション行列
		Matrix4x4 World;				  // ワールド行列
		Matrix4x4 WorldInversedTranspose; // ワールド行列の逆転置行列
	};

public: /// ---------- メンバ変数 ---------- ///

	// ローカルスケール
	Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

	// ローカル回転角
	Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

	// ローカル座標
	Vector3 translate_ = { 0.0f, 0.0f, 0.0f };

	// ワールド座標
	Vector3 worldTranslate_ = { 0.0f, 0.0f, 0.0f };

	// ワールド回転
	Vector3 worldRotate_ = { 0.0f, 0.0f, 0.0f };

	// ワールド変換行列
	Matrix4x4 matWorld_;

	// 親となるワールド変換ポインタ
	const WorldTransform* parent_ = nullptr;

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update();

	/// <summary>
	/// 指定したルートパラメータのインデックスでパイプラインを設定
	/// </summary>
	/// <param name="rootParameterIndex">設定するルートパラメータのインデックス</param>
	void SetPipeline(UINT rootParameterIndex = 1);

	/// <summary>
	/// オブジェクトのワールド行列への定数参照を返します。
	/// </summary>
	/// <returns>Matrix4x4 型の、内部メンバ matWorld_ が保持するワールド行列への定数参照。</returns>
	const Matrix4x4& GetWorldMatrix() const { return matWorld_; }

private: /// ---------- メンバ変数 ---------- ///

	// 座標変換行列データ
	ComPtr <ID3D12Resource> wvpResource;	 // WVP行列データ用リソース
	TransformationMatrix* wvpData = nullptr; // WVP行列データのマッピング先ポインタ
};

} // namespace Ken4lowEngine
