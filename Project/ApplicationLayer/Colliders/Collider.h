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

public: /// ---------- デバッグ用メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- 設定 ---------- ///

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

public: /// ---------- 取得 ---------- ///

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

private: /// ---------- メンバ変数 ---------- ///

	// 識別ID
	uint32_t typeID_ = 0u;

};

