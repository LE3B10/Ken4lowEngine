#include "GameOverScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void GameOverScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameOverScene::Update()
{
}

void GameOverScene::Draw3DObjects()
{
}

void GameOverScene::Draw2DSprites()
{
}


/// -------------------------------------------------------------
///				　			終了処理
/// -------------------------------------------------------------
void GameOverScene::Finalize()
{
}


/// -------------------------------------------------------------
///				　		ImGui描画処理
/// -------------------------------------------------------------
void GameOverScene::DrawImGui()
{
}
