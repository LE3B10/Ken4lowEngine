#pragma once
#include "IGameState.h"
#include "GamePlayState.h"
#include "MenuState.h"


/// -------------------------------------------------------------
///				ゲーム全体の状態を管理するクラス
/// -------------------------------------------------------------
class GameStateManager
{
private: /// ---------- メンバ変数 ---------- ///

	// 現在のゲーム状態を表す
	IGameState* currentState;

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ：初期状態nullを設定
	GameStateManager() : currentState(nullptr) {}

	// デストラクタ
	~GameStateManager();

	// 状態変更
	void ChangeState(IGameState* newState);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

};

