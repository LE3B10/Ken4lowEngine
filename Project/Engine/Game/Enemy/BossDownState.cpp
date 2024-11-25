#include "BossDownState.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossDownState::Initialize()
{
	OutputDebugStringA("Boss enters Down State. Temporarily vulnerable!\n");
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossDownState::Update()
{
	OutputDebugStringA("Boss is down Cannot act!\n");
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void BossDownState::Draw()
{
	OutputDebugStringA("Rendering Boss in Down State\n");
}


/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossDownState::Exit()
{
	OutputDebugStringA("Boss exits Down State\n");
}
