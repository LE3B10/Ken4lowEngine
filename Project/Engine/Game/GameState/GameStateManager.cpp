#include "GameStateManager.h"


/// -------------------------------------------------------------
///						デストラクタ
/// -------------------------------------------------------------
GameStateManager::~GameStateManager()
{
	// 現在の状態が存在する場合、終了処理を行い、メモリ開放
	if (currentState)
	{
		currentState->Exit(); // 終了処理の呼び出し
		delete currentState;  // メモリ開放
	}
}


/// -------------------------------------------------------------
///							状態変更
/// -------------------------------------------------------------
void GameStateManager::ChangeState(IGameState* newState)
{
	// 現在の状態が存在する場合、終了処理を行い、メモリ開放
	if (currentState)
	{
		currentState->Exit(); // 現在の状態の終了処理
		delete currentState;  // メモリ開放
	}

	// 新しい状態を設定
	currentState = newState;

	// 新しい状態が有効なら初期化処理を呼び出す
	if (currentState)
	{
		currentState->Initialize();
	}
}


/// -------------------------------------------------------------
///				ゲーム全体の状態を管理するクラス
/// -------------------------------------------------------------
void GameStateManager::Update()
{
	if (currentState)
	{
		currentState->Update(); // 現在の状態の更新処理を呼び出す
	}
}


/// -------------------------------------------------------------
///				ゲーム全体の状態を管理するクラス
/// -------------------------------------------------------------
void GameStateManager::Draw()
{
	if (currentState)
	{
		currentState->Draw(); // 現在の状態の描画処理を呼び出す
	}
}
