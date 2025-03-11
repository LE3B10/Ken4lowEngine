#pragma once
#include <cstdint>

#include "WorldTransform.h"
#include "Vector3.h"
#include <Vector4.h>

/// -------------------------------------------------------------
///						　当たり判定クラス
/// -------------------------------------------------------------
class Collider
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// 仮想デストラクタ
	virtual ~Collider() = default;

	// 衝突時に呼ばれる仮想関数
	virtual void OnCollision([[maybe_unused]] Collider* other) {}

	// 中心座標を取得する純粋仮想関数
	virtual Vector3 GetCenterPosition() const = 0;

public: /// ---------- デバッグ用メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- 取得 ---------- ///

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

	// 半径を取得
	float GetRadius() const { return radius_; }

	// 色を取得
	Vector4 GetColor() const { return defaultColor_; }

public: /// ---------- 設定 ---------- ///

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

	// 半径を設定
	void SetRadius(float radius) { radius_ = radius; }

	// 色を設定
	void SetColor(const Vector4& color) { defaultColor_ = color; }

	// 回転を設定
	void SetRotation(const Vector3& rotation) { rotation_ = rotation; }

	// 親座標を設定
	void SetParentPosition(const Vector3& position) { parentPosition_ = position; }

private: /// ---------- メンバ変数 ---------- ///

	// 衝突半径
	float radius_ = 1.0f;

	// 識別ID
	uint32_t typeID_ = 0u;

	// コライダーの色
	Vector4 defaultColor_ = { 1.0f,1.0f,1.0f,1.0f }; // 白

	Vector3 parentPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 worldPosition_ = {};
	Vector3 rotation_ = {};
};

