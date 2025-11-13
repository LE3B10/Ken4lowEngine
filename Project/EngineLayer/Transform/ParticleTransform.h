#pragma once
#include <DX12Include.h>
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Camera.h"


/// -------------------------------------------------------------
///				パーティクル用の座標変換データクラス
/// -------------------------------------------------------------
class ParticleTransform
{
public: /// ---------- メンバ変数 ---------- ///

	Vector3 scale_ = { 1.0f, 1.0f, 1.0f };	   // スケール
	Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };	   // 回転
	Vector3 translate_ = { 0.0f, 0.0f, 0.0f }; // 平行移動

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ビュー・プロジェクション行列を基に行列を更新します。useBillboard が true の場合は billboardMatrix を適用してビルボード変換を行います。
	/// </summary>
	/// <param name="viewProjection">ビュー行列とプロジェクション行列を掛け合わせた行列（読み取り専用）。更新に使用される基準変換行列。</param>
	/// <param name="useBillboard">ビルボード変換を適用するかどうか。true の場合 billboardMatrix が使用されます。</param>
	/// <param name="billboardMatrix">ビルボード変換に使用する行列（読み取り専用）。useBillboard が false の場合は無視されます。</param>
	void UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix);

	/// <summary>
	/// オブジェクトのワールド変換行列（Matrix4x4）への const 参照を返します。メソッドは const であり、オブジェクトの状態を変更しません。
	/// </summary>
	/// <returns>worldMatrix_ を指す const Matrix4x4 への参照。呼び出し側で行列を変更しないことを意図した参照です。</returns>
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	/// <summary>
	/// WVP（ワールド・ビュー・プロジェクション）行列への定数参照を返します。
	/// </summary>
	/// <returns>Matrix4x4 型で表される WVP 行列への const 参照。</returns>
	const Matrix4x4& GetWVPMatrix() const { return wvpMatrix_; }

private: /// ---------- メンバ変数 ---------- ///

	// ワールド変換行列
	Matrix4x4 worldMatrix_ = Matrix4x4::MakeIdentity();

	// WVP行列
	Matrix4x4 wvpMatrix_ = Matrix4x4::MakeIdentity();
};

