#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

/// -------------------------------------------------------------
///				　	拡張ワールド変換クラス
/// -------------------------------------------------------------
class WorldTransformEx
{
public: /// ---------- メンバ関数 ---------- ///

	// 更新処理
	void Update();

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
	Matrix4x4 worldMatrix_;

	// 親となるワールド変換ポインタ
	const WorldTransformEx* parent_ = nullptr;
};

