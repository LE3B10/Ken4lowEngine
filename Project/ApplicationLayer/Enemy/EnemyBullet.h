#pragma once
#include <Object3D.h>
#include <Vector3.h>
#include <Collider.h>
#include <ContactRecord.h>


/// -------------------------------------------------------------
///				　			敵弾クラス
/// -------------------------------------------------------------
class EnemyBullet : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- セッター ---------- ///

	// 位置を設定
	void SetPosition(const Vector3& pos) { position_ = pos; }

	// 速度を設定
	void SetVelocity(const Vector3& vel) { velocity_ = vel; }

public: /// ---------- ゲッター ---------- ///

	// 寿命を取得
	bool IsDead() const { return lifeTime_ <= 0.0f || isDead_; }

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return position_; }

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// シリアルナンバーを取得
	uint32_t GetSerialNumber() const { return serialNumber_; }

	// Segmentを返す（Colliderの仮想関数をオーバーライド）
	Segment GetSegment() const override { return segment_; }

private: /// ---------- メンバ変数 ---------- ///

	ContactRecord contactRecord_; // 衝突記録

	std::unique_ptr<Object3D> model_; // モデル描画用
	Vector3 position_ = {}; 		  // 現在位置
	Vector3 velocity_ = {}; 		  // 速度（移動方向）
	float lifeTime_ = 0.5f; 		  // 寿命（秒）
	bool isDead_ = false; 			  // 死亡フラグ

	Segment segment_; // 弾が保持する線分
	Vector3 previousPosition_; // 前回の位置（弾の移動を追跡するため）
};

