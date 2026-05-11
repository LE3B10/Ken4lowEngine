#define NOMINMAX
#include "SceneManager.h"

#include <SpriteManager.h>
#include <GameTimer.h>
#include <Editor/EditorPlayController.h>

#include <cassert>
#include <algorithm>

namespace K4E = ::Ken4lowEngine;

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
	fadeManager_ = std::make_unique<FadeManager>();
	fadeManager_->Initialize();

	isTransitioning_ = false;
	sceneSwapped_ = false;
	pendingCrack_ = false;

	coverHoldCounter_ = 0;
	uncoverDelayCounter_ = 0;
	unloadRequested_ = false;
}

void SceneManager::Update()
{
	float dtRaw = K4E::GameTimer::GetInstance()->GetDeltaTime();
	float dtFade = std::min(dtRaw, 1.0f / 30.0f);

	if (fadeManager_)
	{
		fadeManager_->Update(dtFade);
	}

	if (isTransitioning_)
	{
		// ----------------------------
		// 旧シーンの覆い・段階解放
		// ----------------------------
		if (!sceneSwapped_)
		{
			if (fadeManager_ && fadeManager_->IsFullyCovered() && nextScene_)
			{
				if (scene_)
				{
					if (!unloadRequested_)
					{
						scene_->StartUnload();
						unloadRequested_ = true;
					}

					scene_->UpdateUnload();
				}

				const bool readyToSwap = (!scene_) || scene_->IsReadyToSwapOut();

				if (readyToSwap)
				{
					++coverHoldCounter_;

					if (coverHoldCounter_ >= coverHoldFrames_)
					{
						ApplyNextScene();
						sceneSwapped_ = true;
						pendingCrack_ = true;

						coverHoldCounter_ = 0;
						uncoverDelayCounter_ = 0;
						unloadRequested_ = false;
					}
				}
				else
				{
					coverHoldCounter_ = 0;
				}
			}
			else
			{
				coverHoldCounter_ = 0;
			}
		}

		// ----------------------------
		// 新シーンの段階ロード
		// ----------------------------
		if (pendingCrack_ && fadeManager_ && fadeManager_->IsFullyCovered() && scene_)
		{
			scene_->UpdateLoad();

			if (scene_->IsReadyToStartUncover())
			{
				++uncoverDelayCounter_;

				if (uncoverDelayCounter_ >= uncoverDelayFrames_)
				{
					fadeManager_->StartCrack();
					pendingCrack_ = false;
					uncoverDelayCounter_ = 0;
				}
			}
			else
			{
				uncoverDelayCounter_ = 0;
			}
		}

		// ----------------------------
		// 遷移終了
		// ----------------------------
		if (fadeManager_ && !fadeManager_->IsBusy())
		{
			isTransitioning_ = false;
			sceneSwapped_ = false;
			coverHoldCounter_ = 0;
			uncoverDelayCounter_ = 0;
			unloadRequested_ = false;

			if (hasQueuedChange_)
			{
				std::string name = queuedSceneName_;
				hasQueuedChange_ = false;
				queuedSceneName_.clear();
				ChangeScene(name);
			}
		}
	}

	// Play中だけゲームUpdateを進め、Edit/Pause中はEditor確認用更新に切り替える。
	if (scene_)
	{
		if (!isTransitioning_ || sceneSwapped_)
		{
			if (K4E::EditorPlayController::GetInstance()->IsPlaying())
			{
				scene_->Update();
			}
			else
			{
				scene_->UpdateEditor(dtRaw);
			}
		}
	}
}

void SceneManager::Draw3DObjects()
{
	if (scene_)
	{
		scene_->Draw3DObjects();
	}
}

void SceneManager::DrawShadowObjects()
{
	if (scene_)
	{
		scene_->DrawShadowObjects();
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
		K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
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

	if (IsTransitioning())
	{
		queuedSceneName_ = sceneName;
		hasQueuedChange_ = true;
		return;
	}

	nextScene_ = sceneFactory_->CreateScene(sceneName);

	if (!fadeManager_)
	{
		ApplyNextScene();
		return;
	}

	fadeManager_->StartCover();
	isTransitioning_ = true;
	sceneSwapped_ = false;
	pendingCrack_ = false;

	coverHoldCounter_ = 0;
	uncoverDelayCounter_ = 0;
	unloadRequested_ = false;
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

	if (scene_)
	{
		scene_->SetSceneManager(this);
		scene_->Initialize();   // 軽い初期化だけ
		scene_->StartLoad();    // 重いロードの開始
	}
}