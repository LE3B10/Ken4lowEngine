#pragma once
#include <Object3D.h>
#include <WorldTransform.h>

#include <vector>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class Camera;
class Input;


/// ---------- 部位データ構造体 ---------- ///
struct BodyPart
{
	std::unique_ptr<Object3D> object;
	WorldTransform transform;
};


/// -------------------------------------------------------------
///					プレイヤークラス
/// -------------------------------------------------------------
class Player
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- ゲッタ ---------- ///

	// カメラの取得
	Camera* GetCamera() { return camera_; }

	// プレイヤーのワールド変換の取得
	const WorldTransform* GetWorldTransform() { return &body_.transform; }

public: /// ---------- セッタ ---------- ///

	// カメラの設定
	void SetCamera(Camera* camera) { camera_ = camera; }

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;

	// カメラ
	Camera* camera_ = nullptr;

	// 体（親）
	BodyPart body_;

	// 他の部位（子）
	std::vector<BodyPart> parts_;

	// 移動速度
	float moveSpeed_ = 0.3f;

};

