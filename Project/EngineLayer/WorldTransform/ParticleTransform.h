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

	// 更新処理
	void UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix);

	/// @brief ワールド行列の取得
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

	/// @brief WVP行列の取得
	const Matrix4x4& GetWVPMatrix() const { return wvpMatrix_; }

private: /// ---------- メンバ変数 ---------- ///

	Matrix4x4 worldMatrix_ = Matrix4x4::MakeIdentity();
	Matrix4x4 wvpMatrix_ = Matrix4x4::MakeIdentity();

};

