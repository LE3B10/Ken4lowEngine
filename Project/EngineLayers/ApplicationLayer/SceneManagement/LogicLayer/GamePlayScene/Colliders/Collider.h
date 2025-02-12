#pragma once
#include <cstdint>
#include <memory>

#include "Vector3.h"
#include "WorldTransform.h"
#include "Object3d.h"


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

public: /// ---------- 設定 ---------- ///

	// 半径を設定
	void SetRadius(float radius) { radius_ = radius; }

public:	/// ---------- 取得 ---------- ///

	// 半径を取得
	float GetRadius() { return radius_; }

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理・可視化処理用
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理・可視化処理用
	void Update();

	// 描画処理・可視化処理用
	void Draw();

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

private: /// ---------- メンバ変数 ---------- /// 

	// 衝突半径
	float radius_ = 1.5f;

protected: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object3D_;

	// 識別ID
	uint32_t typeID_ = 0u;

};

