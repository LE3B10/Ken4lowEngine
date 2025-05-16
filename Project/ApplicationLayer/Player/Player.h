#pragma once
#include <BaseCharacter.h>
#include <PlayerController.h>
#include "Weapon.h"

/// ---------- 前方宣言 ---------- ///
class FollowCamera;


//// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// ImGui描画処理
	void DrawImGui();

private: /// ---------- メンバ関数 ---------- ///

	// プレイヤー専用パーツの初期化
	void InitializeParts() override;

	// 移動処理
	void Move();

	// 腕のアニメーション
	void UpdateArmAnimation(bool isMoving);

public: /// ---------- ゲッタ ---------- ///

	// プレイヤーの向きを取得
	float GetYaw() const { return body_.worldTransform_.rotate_.y; }

public: /// ---------- セッタ ---------- ///

	// 追従カメラを設定
	void SetCamera(Camera* camera) { camera_ = camera; }

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力クラス
	Camera* camera_ = nullptr; // カメラクラス

	std::unique_ptr<PlayerController> controller_; // プレイヤーコントローラー
	std::unique_ptr<Weapon> weapon_; // 武器クラス

private: /// ---------- ジャンプ機能 ---------- ///

	Vector3 velocity_ = {};        // 移動速度（Y成分がジャンプに使われる）
	bool isGrounded_ = true;       // 地面にいるかどうか
	const float gravity_ = -0.98f; // 重力加速度
	const float jumpPower_ = 0.3f; // ジャンプ力

	float deltaTime = 1.0f / 60.0f; // フレーム間時間（例: 1/60秒）

private: /// ---------- プレイヤーの状態 ---------- ///

	float armSwingParameter_ = 0.0f; // 腕のアニメーション用の媒介変数（フレームごとに変化）
};

