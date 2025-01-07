#include "GameResultScene.h"
#include <Windows.h>
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include "SceneManager.h"

void GameResultScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();

	// テクスチャのパスをリストで管理
	texturePaths_ = {
		"Resources/GameResult.png",
	};

	/// ---------- TextureManagerの初期化 ----------///
	for (auto& texture : texturePaths_)
	{
		textureManager->LoadTexture(texture);
	}

	/// ---------- Spriteの初期化 ---------- ///
	for (uint32_t i = 0; i < 1; i++)
	{
		sprites_.push_back(std::make_unique<Sprite>());

		// テクスチャの範囲をチェック
		if (!texturePaths_.empty())
		{
			sprites_[i]->Initialize(texturePaths_[i % texturePaths_.size()]);
		}
		else
		{
			throw std::runtime_error("Texture paths list is empty!");
		}

		sprites_[i]->SetPosition(Vector2(100.0f * i, 100.0f * i));
	}
}

void GameResultScene::Update()
{
	// キー入力などでタイトルシーンに戻る例
	if (input->PushKey(DIK_ESCAPE)) // 例: プレイヤーがキーを押した場合
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("TitleScene"); // シーン名を指定して変更
		}
	}

	if (input->PushKey(DIK_RETURN)) // 例: プレイヤーがキーを押した場合
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("GamePlayScene"); // シーン名を指定して変更
		}
	}

	// スプライトの更新処理
	for (auto& sprite : sprites_)
	{
		sprite->Update();
	}
}

void GameResultScene::Draw()
{
	// 結果画面の描画処理
	// スプライトの更新処理
	for (auto& sprite : sprites_)
	{
		sprite->Draw();
	}
}

void GameResultScene::Finalize()
{
	if (!sprites_.empty())
	{
		sprites_.clear();
	}

	if (wavLoader_)
	{
		wavLoader_.reset();
	}
}

void GameResultScene::DrawImGui()
{

}
