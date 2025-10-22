#pragma once
#include "Vector3.h"
#include "Object3D.h"
#include "Collider.h"
#include "ItemType.h"

#include <memory>

/// -------------------------------------------------------------
///						アイテムクラス
/// -------------------------------------------------------------
class Item : public Collider
{
public: /// ---------- メンバ関数 --------- ///

	// 初期化処理
	void Initialize(ItemType type, const Vector3& pos);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// 衝突判定
	bool CheckCollisionWithPlayer(const Vector3& playerPos);

	// 効果適用
	void ApplyTo(class Player* player); // 効果適用

	// アイテムの種類を取得
	bool IsCollected() const { return collected_; }
	bool IsExpired() const { return lifetime_ >= maxLifetime_; }

public: /// ---------- オーバーライド ---------- ///

	// 衝突時の処理
	void OnCollision(Collider* other) override;

	// 中心座標を取得
	Vector3 GetCenterPosition() const override { return position_; }

	// 中心座標を設定
	void SetCenterPosition(const Vector3& pos) override { position_ = pos; }

	Vector3 GetOBBHalfSize() const override { return scale_; } // アイテムの大きさに応じて調整}
	void SetOBBHalfSize(const Vector3& halfSize) override { scale_ = halfSize; }

	// 回転（オイラー角）取得・設定
	Vector3 GetOrientation() const override { return rotation_; }
	void SetOrientation(const Vector3& rot) override { rotation_ = rot; }

private: /// ---------- メンバ変数 ---------- ///

	// アイテムの種類
	ItemType type_ = {};

	// スプライト
	std::unique_ptr<Object3D> object3d_;

	// アイテムの位置
	Vector3 position_ = {};

	// 収集済みフラグ
	bool collected_ = false;

	// アイテムの大きさ（スケール）
	Vector3 scale_ = { 1.0f, 1.0f, 1.0f }; // スケール（大きさ）設定

	float floatTimer_ = 0.0f;     // サイン波用タイマー
	float floatAmplitude_ = 0.6f; // 上下振幅（移動幅）
	float floatSpeed_ = 4.0f;     // 周期スピード
	Vector3 basePosition_;        // 元の位置（初期位置）

	// 回転用の変数
	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 回転用の変数
	// 回転速度
	float rotationSpeed_ = 0.01f; // 回転速度（ラジアン単位）

	float lifetime_ = 0.0f;
	const float maxLifetime_ = 999.0f; // 秒単位で存在限界時間
};
