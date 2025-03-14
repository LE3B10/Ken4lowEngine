#include "GameClearScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>
#include <SceneManager.h>


void GameClearScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input_ = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();
}

void GameClearScene::Update()
{
	if (input_->TriggerKey(DIK_RETURN) || input_->TriggerButton(XButtons.A))
	{
		sceneManager_->ChangeScene("GameTitleScene"); // リトライ
	}


}

void GameClearScene::Draw()
{

}

void GameClearScene::Finalize()
{
	ParticleManager::GetInstance()->Finalize();
}

void GameClearScene::DrawImGui()
{

}
