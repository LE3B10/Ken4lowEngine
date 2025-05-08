#include "SceneManager.h"


/// -------------------------------------------------------------
///					　	シングルトンインスタンス
/// -------------------------------------------------------------
SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}


/// -------------------------------------------------------------
///						　	更新処理
/// -------------------------------------------------------------
void SceneManager::Update()
{
	// 次のシーンが設定されている場合
	if (nextScene_)
	{
		// 現在のシーンを終了する
		if (scene_)
		{
			scene_->Finalize();
		}

		// 次のシーンを現在のシーンとしてセット
		scene_ = std::move(nextScene_);

		// シーンマネージャーをセット
		scene_->SetSceneManager(this);

		// 新しいシーンを初期化
		scene_->Initialize();
	}

	// 現在のシーンを更新
	if (scene_)
	{
		scene_->Update();
	}
}

void SceneManager::Draw3DObjects()
{
	if (scene_) {
		scene_->Draw3DObjects();
	}
}

void SceneManager::Draw2DSprites()
{
	if (scene_) {
		scene_->Draw2DSprites();
	}
}


/// -------------------------------------------------------------
///					　		描画処理
/// -------------------------------------------------------------
void SceneManager::DrawImGui()
{
	// 現在のシーンを描画
	if (scene_)
	{
		scene_->DrawImGui();
	}
}


/// -------------------------------------------------------------
///					　		終了処理
/// -------------------------------------------------------------
void SceneManager::Finalize()
{
	if (scene_)
	{
		scene_->Finalize();
	}
}


/// -------------------------------------------------------------
///					　		終了処理
/// -------------------------------------------------------------
void SceneManager::ChangeScene(const std::string& sceneName)
{
	assert(sceneFactory_);
	assert(nextScene_ == nullptr);

	// 次のシーン生成
	nextScene_ = sceneFactory_->CreateScene(sceneName);
}
