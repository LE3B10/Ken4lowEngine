#include "TitleScene.h"
#include <DirectXCommon.h>
#include <SpriteManager.h>
#include <Object3DCommon.h>
#include <ImGuiManager.h>
#include "SceneManager.h"
#include <CollisionUtility.h>
#include <Wireframe.h>

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	input = Input::GetInstance();

	// テクスチャのパスをリストで管理
	texturePaths_ = {
		"Resources/uvChecker.png",
		"Resources/monsterBall.png",
	};

	/// ---------- Spriteの初期化 ---------- ///
	for (uint32_t i = 0; i < texturePaths_.size(); i++)
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
	}

	if (input->TriggerKey(DIK_BACK))
	{
		if (sceneManager_)
		{
			sceneManager_->ChangeScene("PhysicalScene"); // 戻るキーでゲームプレイシーンに戻る
		}
	}

	// スプライトの更新処理
	for (auto& sprite : sprites_)
	{
		sprite->Update();
	}
}


/// -------------------------------------------------------------
///				　	3Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw3DObjects()
{
#pragma region オブジェクト3Dの描画

	// オブジェクト3D共通描画設定
	Object3DCommon::GetInstance()->SetRenderSetting();

#pragma endregion

}


/// -------------------------------------------------------------
///				　	2Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw2DSprites()
{
#pragma region 背景の描画（後面）

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();

	/// ----- スプライトの描画設定と描画 ----- ///
	for (auto& sprite : sprites_)
	{
		sprite->Draw();
	}

#pragma endregion


#pragma region UIの描画（前面）
	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

#pragma endregion

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
