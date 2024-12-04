#include "TitleScene.h"
#include <DirectXCommon.h>
#include <ImGuiManager.h>
#include "SceneManager.h"

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	textureManager = TextureManager::GetInstance();
	input = Input::GetInstance();
	wavLoader_ = std::make_unique<WavLoader>();

	// テクスチャのパスをリストで管理
	texturePaths_ = {
		"Resources/monsterBall.png",
		//"Resources/uvChecker.png",
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

	/// ---------- サウンドの初期化 ---------- ///
	const char* fileName = "Resources/Sounds/RPGBattle01.wav";
	wavLoader_->StreamAudioAsync(fileName, 0.5f, 1.0f, false);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	// 入力によるシーン切り替え
	if (input->TriggerKey(DIK_RETURN)) // Enterキーが押されたら
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("GamePlayScene"); // シーン名を指定して変更
		}

		wavLoader_->StopBGM();
	}

	// スプライトの更新処理
	for (auto& sprite : sprites_)
	{
		sprite->Update();
	}
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void TitleScene::Draw()
{
	/// ----- スプライトの描画設定と描画 ----- ///
	for (auto& sprite : sprites_)
	{
		sprite->Draw();
	}
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void TitleScene::Finalize()
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


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void TitleScene::DrawImGui()
{
	ImGui::Begin("Test Window");

	for (uint32_t i = 0; i < sprites_.size(); i++)
	{
		ImGui::PushID(i); // スプライトごとに異なるIDを設定
		if (ImGui::TreeNode(("Sprite" + std::to_string(i)).c_str()))
		{
			Vector2 position = sprites_[i]->GetPosition();
			ImGui::DragFloat2("Position", &position.x, 1.0f);
			sprites_[i]->SetPosition(position);

			float rotation = sprites_[i]->GetRotation();
			ImGui::SliderAngle("Rotation", &rotation);
			sprites_[i]->SetRotation(rotation);

			Vector2 size = sprites_[i]->GetSize();
			ImGui::DragFloat2("Size", &size.x, 1.0f);
			sprites_[i]->SetSize(size);

			ImGui::TreePop();
		}
		ImGui::PopID(); // IDを元に戻す
	}

	ImGui::End();
}
