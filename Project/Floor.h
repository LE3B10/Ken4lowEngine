#pragma once
#include "Object3D.h"
#include "WorldTransform.h"
#include "Collider.h"
#include "ContactRecord.h"

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;


/// -------------------------------------------------------------
///						床クラス
/// -------------------------------------------------------------
class Floor : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Floor();

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 接触記録を削除
	void ClearContactRecord();

	// 衝突時に呼ばれる関数
	void OnCollision([[maybe_unused]] Collider* other) override;

public: /// ---------- オーバーライド関数 （ゲッタ） ---------- ///

	// 中心座標を取得
	Vector3 GetCenterPosition() const { return worldTransform_.translate; }

	// サイズを取得
	Vector3 GetSize() const { return worldTransform_.scale; }

	/// <summary>
	/// 向き（Orientation）を取得
	/// </summary>
	/// <param name="index">番号 0: X軸, 1: Y軸, 2: Z軸 </param>
	/// <returns></returns>
	Vector3 GetOrientation(int index) const;

public: /// ---------- メンバ変数 ---------- ///

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	// オブジェクト3D
	std::unique_ptr<Object3D> object3D_;

	// 接触記録
	ContactRecord contactRecord_;

	// シリアルナンバー
	uint32_t serialNumber_ = 0;
	// 次のシリアルナンバー
	static inline uint32_t nextSerialNumber_ = 0;
};

