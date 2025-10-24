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

	/// <summary>
	/// 親のワールド変換と指定されたオフセットや回転を基に、オブジェクトのワールド変換を更新する。
	/// </summary>
	/// <param name="parent">親の WorldTransformEx を指すポインタ。親が存在しない場合は nullptr を渡すことができる。</param>
	/// <param name="offset">親に対する位置のオフセットを表す Vector3 の参照。</param>
	/// <param name="preRotate">更新前に適用する回転角X（float）。単位は実装依存。</param>
	/// <param name="selfAdd">自身の変換に加算する Vector3 の参照（位置やその他の調整用）。</param>
	void Update(const WorldTransformEx* parent, const Vector3& offset, float preRotateX, const Vector3& selfAdd);

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

