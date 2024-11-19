#include "IdleState.h"

#include "Input.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void IdleState::Initialize()
{
	OutputDebugStringA("Entering Idle State\n");
}



/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void IdleState::Update()
{
	OutputDebugStringA("Updating Idle State\n");
}



/// -------------------------------------------------------------
///							入力処理
/// -------------------------------------------------------------
void IdleState::HandleInput(Input* input)
{
	OutputDebugStringA("Trnasition to Running State\n");
}



/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void IdleState::Draw()
{
	OutputDebugStringA("Rendering Idle State\n");
}



/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void IdleState::Exit()
{
	OutputDebugStringA("Exiting Idle State\n");
}
