#pragma once
#include "Sprite.h"
#include <Input.h>
#include <list>
#include <memory>
#include <numbers>
#include "WorldTransform.h"
#include "Camera.h"
#include <cmath>

/// ---------- 前方宣言 ---------- ///
class Enemy;


/// -------------------------------------------------------------
///				　		ロックオンクラス
/// -------------------------------------------------------------
class LockOn
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Camera* camera);

	// 更新処理
	void Update(const std::list<std::unique_ptr<Enemy>>& enemies, Camera* camera);

	// 描画処理
	void Draw();

	// 中心座標
	Vector3 GetTargetPosition() const;

	// ロックオン中かどうかを確認するための関数
	bool ExistTarget() const { return target_ ? true : false; }

private: /// ---------- メンバ変数 ---------- ///

	Vector2 ConvertToScreenPosition(const Vector3& worldPos);

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;
	Camera* camera_ = nullptr;

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	// ロックオンマーク用スプライト
	std::unique_ptr<Sprite> lockOnMark_;

	// ロックオン対象
	const Enemy* target_ = nullptr;

	// 最小距離
	float minDistance_ = 0.0f;
	// 最大距離
	float maxDistance_ = 100.0f;
	// 角度範囲
	float angleRange = 20.0f * std::numbers::pi_v<float>;

	// ロックオン状態
	bool isLockedOn_ = false;

	// Rキーの前回の状態
	bool wasRKyePresses_ = false;
};
