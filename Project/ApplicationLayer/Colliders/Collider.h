#pragma once
#include <cstdint>
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "IDGenerator.h"

#include "OBB.h"


/// -------------------------------------------------------------
///                     当たり判定クラス
/// -------------------------------------------------------------
class Collider
{
public: /// ---------- 純粋仮想関数 ---------- ///

	// コンストラクタ
	Collider() : serialNumber_(IDGenerator::Generate()) {} // シリアルナンバーを生成

	// 仮想デストラクタ
	virtual ~Collider() = default;

	// 衝突時に呼ばれる仮想関数
	virtual void OnCollision([[maybe_unused]] Collider* other) {}

	// 中心座標取得・設定
	virtual Vector3 GetCenterPosition() const { return colliderPosition_; }
	virtual void SetCenterPosition(const Vector3& pos) { colliderPosition_ = pos; }

	// 半サイズ取得・設定
	virtual Vector3 GetOBBHalfSize() const { return colliderHalfSize_; }
	virtual void SetOBBHalfSize(const Vector3& halfSize) { colliderHalfSize_ = halfSize; }

	// 回転（オイラー角）取得・設定
	virtual Vector3 GetOrientation() const { return orientation_; }
	virtual void SetOrientation(const Vector3& rot) { orientation_ = rot; }

	OBB GetOBB() const;

	// シリアルナンバーを取得
	uint32_t GetUniqueID() const { return serialNumber_; }

public: /// ---------- デバッグ用メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理（OBBの可視化）
	void Draw();

	// ImGui描画処理
	void DrawImGui();

public: /// ---------- 設定 ---------- ///

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

private: /// ---------- メンバ変数 ---------- ///

	// 識別ID
	uint32_t typeID_ = 0u;

	// OBBの半サイズ
	Vector3 colliderPosition_ = { 0.0f, 0.0f, 0.0f };
	Vector3 colliderHalfSize_ = { 1.0f, 1.0f, 1.0f };
	Vector3 orientation_ = { 0.0f, 0.0f, 0.0f };
	Vector4 debugColor_ = { 1.0f, 0.0f, 0.0f, 1.0f };

protected: /// ---------- シリアルナンバー ---------- ///

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
};
