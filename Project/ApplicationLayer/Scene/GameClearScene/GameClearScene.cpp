#include "GameClearScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>
#include <TextureManager.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void GameClearScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameClearScene::Update()
{
}

void GameClearScene::Draw3DObjects()
{
}

void GameClearScene::Draw2DSprites()
{
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void GameClearScene::Finalize()
{
}


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void GameClearScene::DrawImGui()
{
}
