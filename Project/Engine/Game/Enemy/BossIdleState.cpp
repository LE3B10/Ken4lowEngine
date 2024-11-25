#include "BossIdleState.h"


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossIdleState::Initialize()
{
	OutputDebugStringA("Boss enters Idle State\n");
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossIdleState::Update()
{
	OutputDebugStringA("Boss is idle. Preparing next action.\n");
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void BossIdleState::Draw()
{
	OutputDebugStringA("Rendering Boss in Idle State\n");
}


/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossIdleState::Exit()
{
	OutputDebugStringA("Boss exits Idle State\n");
}
