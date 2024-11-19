#include "EnragedState.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void EnragedState::Initialize()
{
	OutputDebugStringA("Boss enters Enraged State. Power increasses!\n");
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void EnragedState::Update()
{
	OutputDebugStringA("Boss is attacking fiercely in Enraged State!.\n");
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void EnragedState::Draw()
{
	OutputDebugStringA("Rendering Boss in Enraged State\n");
}


/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void EnragedState::Exit()
{
	OutputDebugStringA("Boss exits Enraged State\n");
}
