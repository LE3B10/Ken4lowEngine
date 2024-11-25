#include "GamePlayState.h"


/// -------------------------------------------------------------
///					ゲームプレイの初期化処理
/// -------------------------------------------------------------
void GamePlayState::Initialize()
{
	OutputDebugStringA("Entering GamePlay State\n");
}


/// -------------------------------------------------------------
///					ゲームプレイの更新処理
/// -------------------------------------------------------------
void GamePlayState::Update()
{
	OutputDebugStringA("Updating GamePlay State\n");
}


/// -------------------------------------------------------------
///					ゲームプレイの描画処理
/// -------------------------------------------------------------
void GamePlayState::Draw()
{
	OutputDebugStringA("Rendering GamePlay State\n");
}


/// -------------------------------------------------------------
///					ゲームプレイの終了処理
/// -------------------------------------------------------------
void GamePlayState::Exit()
{
	OutputDebugStringA("Exiting GamePlay State\n");
}
