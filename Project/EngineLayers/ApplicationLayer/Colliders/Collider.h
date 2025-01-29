#pragma once
#include <cstdint>
#include "Vector3.h"
#include "WorldTransform.h"
#include "Object3D.h"

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;

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

	// 初期化処理・可視化処理用
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理・可視化処理用
	void Update();

	// 描画処理・可視化処理用
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// 衝突判定のスケールを取得
	Vector3 GeSacle() const { return worldTransform_.scale; }

	// 衝突判定の回転を取得
	Vector3 GetRotate() const { return worldTransform_.rotate; }

	// 衝突判定の座標を取得
	Vector3 GetTranslate() const { return worldTransform_.translate; }

	// 識別IDを取得
	uint32_t GetTypeID() const { return typeID_; }

public: /// ---------- セッタ ---------- ///

	// 衝突判定のスケールを設定
	void SetScale(const Vector3& scale) { worldTransform_.scale = scale; }

	// 衝突判定の回転を設定
	void SetRotate(const Vector3& rotate) { worldTransform_.rotate = rotate; }

	// 衝突判定の座標を設定
	void SetTranslate(const Vector3& translate) { worldTransform_.translate = translate; }

	// 識別IDを設定
	void SetTypeID(uint32_t typeID) { typeID_ = typeID; }

private: /// ---------- メンバ変数 ---------- ///

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	std::unique_ptr<Object3D> object3D_;

	// 識別ID
	uint32_t typeID_ = 0u;

};

