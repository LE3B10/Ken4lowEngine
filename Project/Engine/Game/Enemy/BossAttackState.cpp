#include "BossAttackState.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void BossAttackState::Initialize()
{
	OutputDebugStringA("Boss enters Attack State\n");
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossAttackState::Update()
{
	OutputDebugStringA("Boss is attacking the player!.\n");
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void BossAttackState::Draw()
{
	OutputDebugStringA("Rendering Boss in Attack State\n");
}


/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void BossAttackState::Exit()
{
	OutputDebugStringA("Boss exits Attack State\n");
}
