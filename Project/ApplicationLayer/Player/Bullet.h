#pragma once
#include <Object3D.h>
#include <Vector3.h>
#include <Collider.h>
#include <ContactRecord.h>

#include <memory>

/// -------------------------------------------------------------
///				　		弾丸クラス
/// -------------------------------------------------------------
class Bullet : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

	// 位置を設定
	void SetPosition(const Vector3& position) { position_ = position; }

	// 速度を設定
	void SetVelocity(const Vector3& velocity) { velocity_ = velocity; }

	// 寿命を取得
	bool IsDead() const { return lifeTime_ <= 0.0f || isDead_; }

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return position_; }

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	void SetDamage(float dmg) { damage_ = dmg; }
	float GetDamage() const { return damage_; }

private: /// ---------- メンバ変数 ---------- ///

	ContactRecord contactRecord_; // 衝突記録

	std::unique_ptr<Object3D> model_; // モデル描画用
	Vector3 position_ = {};           // 現在位置
	Vector3 velocity_ = {};           // 速度（移動方向）
	float lifeTime_ = 3.0f;           // 寿命（秒）
	bool isDead_ = false;            // 死亡フラグ
	float damage_ = 10.0f;  // デフォルト値
};
