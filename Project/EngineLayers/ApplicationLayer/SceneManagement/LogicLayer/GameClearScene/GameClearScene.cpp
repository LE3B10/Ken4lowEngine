#include "GameClearScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>

void GameClearScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}

void GameClearScene::Update()
{
}

void GameClearScene::Draw()
{
}

void GameClearScene::Finalize()
{
}

void GameClearScene::DrawImGui()
{
}
