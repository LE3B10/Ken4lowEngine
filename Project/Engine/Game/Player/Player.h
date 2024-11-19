#pragma once
#include "IdleState.h"
#include "IPlayerState.h"
#include "RunningState.h"

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
///						プレイヤークラス
/// -------------------------------------------------------------
class Player
{
private: /// ---------- メンバ変数 ---------- ///

	// 現在を表すステートパターン
	IPlayerState* currentState;

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	Player() : currentState(nullptr) {}

	// デストラクタ
	~Player();

	// 状態を変更
	void ChangeState(IPlayerState* newState);

	// 更新処理
	void Update();

	// 入力処理
	void HandleInput(Input* input);

	// 描画処理
	void Draw();

};

