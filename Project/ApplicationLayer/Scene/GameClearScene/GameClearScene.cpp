#include "GameClearScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>
#include <TextureManager.h>
#include <SceneManager.h>

namespace K4E = ::Ken4lowEngine;


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void GameClearScene::Initialize()
{
	// カーソルのロックを解除
	K4E::Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true); // 表示・非表示も連動（オプション）

	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input = K4E::Input::GetInstance();
}


/// -------------------------------------------------------------
///				　			更新処理
/// -------------------------------------------------------------
void GameClearScene::Update()
{
	if (input->TriggerKey(DIK_RETURN))
	{
		sceneManager_->ChangeScene("TitleScene");
	}
}


/// -------------------------------------------------------------
///				　			3Dオブジェクトの描画
/// -------------------------------------------------------------
void GameClearScene::Draw3DObjects()
{

}


/// -------------------------------------------------------------
///				　			2Dオブジェクトの描画
/// -------------------------------------------------------------
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
