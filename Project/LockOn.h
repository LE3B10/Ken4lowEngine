#pragma once
#include "Sprite.h"

#include "Vector2.h"
#include "Vector3.h"

#include <cmath>
#include <list>
#include <memory>


/// ---------- 前方宣言 ---------- ///
class Player;
class Enemy;
class FollowCamera;
class Input;


/// -------------------------------------------------------------
///					ロックオンシステムクラス
/// -------------------------------------------------------------
class LockOn
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update(const std::list<std::unique_ptr<Enemy>>& enemies);

	// 描画処理
	void Draw();

public: /// ---------- セッター ---------- ///

	// ロックオンを取得
	bool GetIsLockOn() { return isLockedOn_; }

	// ロックオン対象の座標を取得
	Vector3 GetTargetPosition() const;

	// ロックオン中かを取得
	bool ExistTarget() const { return target_ ? true : false; }

public: /// ---------- セッター ---------- ///

	// プレイヤーとカメラをセットする関数
	void SetPlayerAndCamera(const Player* player, FollowCamera* followCamera)
	{
		player_ = player;
		followCamera_ = followCamera;
	}

private: /// ---------- メンバ関数---------- ///

	// 検索する処理
	void Serching(const std::list<std::unique_ptr<Enemy>>& enemies);

	// 範囲外判定処理
	bool OutOfRangeJudgement(const std::list<std::unique_ptr<Enemy>>& enemies);

	// ワールド座標 -> スクリーン座標
	Vector3 WorldToScreen(const Vector3& positionWorld);

private: /// ---------- メンバ変数 ---------- ///

	const Player* player_ = nullptr;
	FollowCamera* followCamera_ = nullptr;

	// ロックオンマーク用のスプライト
	std::unique_ptr<Sprite> lockOnMark_;

	// ロックオン対象
	const Enemy* target_ = nullptr;

	// 入力
	Input* input_ = nullptr;

	// 最小距離
	float minDistance_ = 10.0f;

	// 最大距離
	float maxDistance_ = 120.0f;

	// 角度範囲
	float angleRange_ = 20.0f;

	// ロックオン状態
	bool isLockedOn_ = false;

	// 前回の状態
	bool wasLockedOn_ = false;
};

