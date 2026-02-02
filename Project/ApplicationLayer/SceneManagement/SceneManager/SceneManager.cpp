#define NOMINMAX
#include "SceneManager.h"

#include <DirectXCommon.h>
#include <SpriteManager.h>

#include <cassert>

/// -------------------------------------------------------------
///                     シングルトンインスタンス
/// -------------------------------------------------------------
SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

SceneManager::~SceneManager() = default;

void SceneManager::Initialize()
{
	// FadeManager を常駐させる
	fadeManager_ = std::make_unique<FadeManager>();
	fadeManager_->Initialize();

	isTransitioning_ = false;
	sceneSwapped_ = false;
}

void SceneManager::Update()
{
	float dt = DirectXCommon::GetInstance()->GetFPSCounter().GetDeltaTime();

	if (fadeManager_) fadeManager_->Update(dt);

	// 例：あなたが「覆ったら切替→Crack開始」をやっている前提
	if (isTransitioning_)
	{
		if (!sceneSwapped_ && fadeManager_ && fadeManager_->IsFullyCovered() && nextScene_)
		{
			ApplyNextScene();
			sceneSwapped_ = true;
			fadeManager_->StartCrack();
		}

		// フェードが終わったら遷移終了
		if (fadeManager_ && !fadeManager_->IsBusy())
		{
			isTransitioning_ = false;
			sceneSwapped_ = false;

			// ★ここが肝：フェード中に来た遷移要求を実行
			if (hasQueuedChange_)
			{
				std::string name = queuedSceneName_;
				hasQueuedChange_ = false;
				queuedSceneName_.clear();
				ChangeScene(name); // ここで次のフェード開始
			}
		}
	}

	// シーン更新（好みで止める/動かす）
	if (scene_)
	{
		// 覆う間は止めたいなら：(!isTransitioning_ || sceneSwapped_) だけ Update
		scene_->Update();
	}
}

void SceneManager::Draw3DObjects()
{
	if (scene_)
	{
		scene_->Draw3DObjects();
	}
}

void SceneManager::Draw2DSprites()
{
	if (scene_)
	{
		scene_->Draw2DSprites();
	}

	// フェードは最後に描画して最前面に
	if (fadeManager_)
	{
		// UI用の共通描画設定
		SpriteManager::GetInstance()->SetRenderSetting_UI();
		fadeManager_->Draw2DSprites();
	}
}

void SceneManager::DrawImGui()
{
	if (scene_)
	{
		scene_->DrawImGui();
	}

	if (fadeManager_)
	{
		fadeManager_->DrawImGui();
	}
}

void SceneManager::Finalize()
{
	if (scene_) { scene_->Finalize(); }

	if (fadeManager_)
	{
		fadeManager_->Finalize();
		fadeManager_.reset();
	}

	nextScene_.reset();
	sceneFactory_.reset();
}

void SceneManager::ChangeScene(const std::string& sceneName)
{
	assert(sceneFactory_);

	// フェード中なら「捨てずに予約」
	if (IsTransitioning())
	{
		queuedSceneName_ = sceneName;
		hasQueuedChange_ = true;
		return;
	}

	// 次シーン生成
	nextScene_ = sceneFactory_->CreateScene(sceneName);

	// フェードマネージャが無いなら即切替（保険）
	if (!fadeManager_)
	{
		ApplyNextScene();
		return;
	}

	// フェード開始
	fadeManager_->StartCover();
	isTransitioning_ = true;
	sceneSwapped_ = false;
}

void SceneManager::ApplyNextScene()
{
	if (!nextScene_) return;

	// 現在のシーンを終了
	if (scene_)
	{
		scene_->Finalize();
	}

	// 差し替え
	scene_ = std::move(nextScene_);

	// シーンマネージャーをセット
	if (scene_)
	{
		scene_->SetSceneManager(this);
	}

	// 初期化
	if (scene_)
	{
		scene_->Initialize();
	}
}