#include "GameResultScene.h"
#include <Windows.h>
#include "SceneManager.h"

void GameResultScene::Initialize()
{
	input = Input::GetInstance();

	OutputDebugStringA("GameResultScene: Initialized\n");
}

void GameResultScene::Update()
{
	OutputDebugStringA("GameResultScene: Updating...\n");

	// キー入力などでタイトルシーンに戻る例
	if (input->PushKey(DIK_RETURN)) // 例: プレイヤーがキーを押した場合
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("TitleScene"); // シーン名を指定して変更
		}
	}
}

void GameResultScene::Draw()
{
	OutputDebugStringA("GameResultScene: Drawing...\n");

	// 結果画面の描画処理
}

void GameResultScene::Finalize()
{
}

void GameResultScene::DrawImGui()
{
}
