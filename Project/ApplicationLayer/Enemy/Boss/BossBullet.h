#pragma once
#include <Object3D.h>
#include <Vector3.h>
#include <Collider.h>
#include <ContactRecord.h>

/// ---------- 前方宣言 ---------- ///
class Boss;


/// -------------------------------------------------------------
///				　			ボスの弾丸クラス
/// -------------------------------------------------------------
class BossBullet : public Collider
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

	// ダメージを設定
	void SetDamage(float dmg) { damage_ = dmg; }

public: /// ---------- ゲッター ---------- ///

	// ダメージを取得
	float GetDamage() const { return damage_; }

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return position_; }

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	// シリアルナンバーを取得
	uint32_t GetSerialNumber() const { return serialNumber_; }

	// Segmentを返す（Colliderの仮想関数をオーバーライド）
	Segment GetSegment() const override { return segment_; }

	// 射程距離設定
	void SetRange(float range) { maxDistance_ = range; }

	// 死亡状態
	bool IsDead() const { return isDead_ || distanceTraveled_ >= maxDistance_; }

	void SetBoss(Boss* boss) { boss_ = boss; } // bossへの参照を設定

private: /// ---------- メンバ変数 ---------- ///

	ContactRecord contactRecord_;         // 衝突記録
	Boss* boss_ = nullptr; // bossへの参照（必要なら）

	std::unique_ptr<Object3D> model_;     // モデル
	Vector3 position_ = {};               // 現在位置
	Vector3 velocity_ = {};               // 速度ベクトル
	Vector3 previousPosition_ = {};       // 1フレーム前の位置
	Segment segment_;                     // 線分（セグメント）
	float damage_ = 50.0f;                // 与ダメージ
	float maxDistance_ = 1000.0f;         // 最大飛距離
	float distanceTraveled_ = 0.0f;       // 現在の飛距離
	bool isDead_ = false;                 // 死亡フラグ
};

