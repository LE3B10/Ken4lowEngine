#include "GameOverScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>
#include "SceneManager.h"


void GameOverScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input_ = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}

void GameOverScene::Update()
{
	if (input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A))
	{
		sceneManager_->ChangeScene("GameTitleScene"); // リトライ
	}


}

void GameOverScene::Draw()
{

}

void GameOverScene::Finalize()
{
	ParticleManager::GetInstance()->Finalize();
}

void GameOverScene::DrawImGui()
{

}
