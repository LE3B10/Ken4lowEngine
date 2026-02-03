#include "GameOverScene.h"
#include <DirectXCommon.h>
#include <Input.h>
#include <ParameterManager.h>
#include <ParticleManager.h>
#include <SceneManager.h>

namespace K4E = ::Ken4lowEngine;


/// -------------------------------------------------------------
///				　			初期化処理
/// -------------------------------------------------------------
void GameOverScene::Initialize()
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
void GameOverScene::Update()
{
	// ゲームオーバー画面の更新処理
	// 例: ボタン入力、アニメーション、エフェクトなど
	if (input->TriggerKey(DIK_RETURN) || input->TriggerMouse(0))
	{
		// ゲームオーバー画面からメインメニューに戻る処理
		sceneManager_->ChangeScene("TitleScene");
	}
}


/// -------------------------------------------------------------
///				　			3Dオブジェクトの描画
/// -------------------------------------------------------------
void GameOverScene::Draw3DObjects()
{

}


/// -------------------------------------------------------------
///				　			2Dオブジェクトの描画
/// -------------------------------------------------------------
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
