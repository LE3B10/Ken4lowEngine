#pragma once
#include <Object3D.h>
#include <Vector3.h>
#include <Collider.h>
#include <ContactRecord.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;


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

	// 射程距離設定
	void SetRange(float range) { maxDistance_ = range; }

	// 死亡状態
	bool IsDead() const { return isDead_ || distanceTraveled_ >= maxDistance_; }

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return position_; }

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override;

	void SetDamage(float dmg) { damage_ = dmg; }
	float GetDamage() const { return damage_; }

	// Segmentを返す（Colliderの仮想関数をオーバーライド）
	Segment GetSegment() const override { return segment_; }

	void SetPlayer(Player* player) { player_ = player; } // プレイヤーへの参照を設定

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへの参照（必要なら）

	static std::shared_ptr<Object3D> sModel_; // 静的モデル（全弾で共有）

	std::unique_ptr<Object3D> model_;     // モデル
	Vector3 position_ = {};               // 現在位置
	Vector3 velocity_ = {};               // 速度ベクトル
	Vector3 previousPosition_ = {};       // 1フレーム前の位置
	Segment segment_;                     // 線分（セグメント）
	float damage_ = 100.0f;                // 与ダメージ
	float maxDistance_ = 1000.0f;         // 最大飛距離
	float distanceTraveled_ = 0.0f;       // 現在の飛距離
	bool isDead_ = false;                 // 死亡フラグ
	ContactRecord contactRecord_;         // 衝突記録
};
