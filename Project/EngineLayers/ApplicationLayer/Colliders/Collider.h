#pragma once
#include <cstdint>
#include "Vector3.h"

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

	/// <summary>
	/// 向き（Orientation）を取得する純粋仮想関数
	/// </summary>
	/// <param name="index">番号 0: X軸, 1: Y軸, 2: Z軸 </param>
	/// <returns></returns>
	virtual Vector3 GetOrientation(int index) const = 0; 

	// サイズを取得する純粋仮想関数
	virtual Vector3 GetSize() const = 0;

public: /// ---------- メンバ関数 ---------- ///

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

private: /// ---------- メンバ変数 ---------- ///

	// 識別ID
	uint32_t typeID_ = 0u;

};

