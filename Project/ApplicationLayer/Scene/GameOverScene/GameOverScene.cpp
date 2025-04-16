#include "GameOverScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>

void GameOverScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}

void GameOverScene::Update()
{
}

void GameOverScene::Draw()
{
}

void GameOverScene::Finalize()
{
}

void GameOverScene::DrawImGui()
{
}
