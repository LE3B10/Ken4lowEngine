#pragma once
#include "Vector3.h"


/// ---------- 前方宣言 ---------- ///
class Input;


/// -------------------------------------------------------------
///					　プレイヤーコントローラー
/// -------------------------------------------------------------
class PlayerController
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 移動入力を取得
	Vector3 GetMoveInput() const { return move_; }

	// ジャンプ入力を取得
	bool IsJumpTriggered() const { return jump_; }

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr;
	Vector3 move_{};
	bool jump_ = false;
};

