#define NOMINMAX
#include "SceneManager.h"

#include <SpriteManager.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif // USE_IMGUI

#include <cassert>
#include <algorithm>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{

	SceneManager::SceneManager() = default;

	SceneManager::~SceneManager() = default;

	void SceneManager::Initialize()
	{
		// 遷移演出はApplicationLayerから注入された場合だけ初期化する。
		if (sceneTransition_)
		{
			sceneTransition_->Initialize();
		}

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

		if (sceneTransition_)
		{
			sceneTransition_->Update(dtFade);
		}

		if (isTransitioning_)
		{
			// ----------------------------
			// 旧シーンの覆い・段階解放
			// ----------------------------
			if (!sceneSwapped_)
			{
				if (sceneTransition_ && sceneTransition_->IsFullyCovered() && nextScene_)
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
			if (pendingCrack_ && sceneTransition_ && sceneTransition_->IsFullyCovered() && scene_)
			{
				scene_->UpdateLoad();

				if (scene_->IsReadyToStartUncover())
				{
					++uncoverDelayCounter_;

					if (uncoverDelayCounter_ >= uncoverDelayFrames_)
					{
						sceneTransition_->StartCrack();
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
			if (sceneTransition_ && !sceneTransition_->IsBusy())
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

		// DebugはPlay状態でUpdateを分離し、ReleaseはEditor状態を見ずに通常Updateする。
		if (scene_)
		{
			if (!isTransitioning_ || sceneSwapped_)
			{
				bool shouldUpdateGame = true;
#ifdef USE_IMGUI
				// Debug/EditorではPlay中だけ通常Updateし、Edit/Pause中はEditor更新に分離する。
				// Game Preview Mode中はEditor UIのPlay状態に依存せず、実ゲーム確認として通常Updateを進める。
				shouldUpdateGame = K4E::EditorModeController::GetInstance()->IsGamePreviewMode() || K4E::EditorPlayController::GetInstance()->IsPlaying();
#else
				// Release/GameではEditorPlayStateを参照せず、常に通常UpdateでTitleSceneを進行させる。
				shouldUpdateGame = true;
#endif // USE_IMGUI
				if (shouldUpdateGame)
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

	void SceneManager::PrepareShadowPass()
	{
		if (scene_)
		{
			scene_->PrepareShadowPass(); // ShadowSystemがCasterを選ぶ前にEditor上の最新LightComponentを同期する。
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

		// シーン遷移演出は最後に描画して最前面にする
		if (sceneTransition_)
		{
			// UI用の共通描画設定
			K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
			sceneTransition_->Draw2DSprites();
		}
	}

	void SceneManager::DrawImGui()
	{
		if (scene_)
		{
			scene_->DrawImGui();
		}

		if (sceneTransition_)
		{
			sceneTransition_->DrawImGui();
		}
	}

	void SceneManager::Finalize()
	{
		if (scene_)
		{
			// Scene固有の終了処理だけを行い、所有権の解放はデストラクタへ任せる。
			scene_->Finalize();
		}

		if (sceneTransition_)
		{
			// 遷移演出の終了処理後も、unique_ptrの破棄はSceneManagerの寿命に任せる。
			sceneTransition_->Finalize();
		}
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

		if (!sceneTransition_)
		{
			ApplyNextScene();
			return;
		}

		sceneTransition_->StartCover();
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

}