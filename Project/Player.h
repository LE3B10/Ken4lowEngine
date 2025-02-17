#pragma once
#include "Object3D.h"
#include "WorldTransform.h"
#include "Camera.h"


/// -------------------------------------------------------------
///					　	プレイヤークラス
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

private: /// ---------- メンバ変数 ---------- ///

	// ワールドトランスフォーム
	WorldTransform worldTransform_;

	// カメラ
	Camera* camera_ = nullptr;

	// オブジェクト
	std::unique_ptr<Object3D> player_;

};

