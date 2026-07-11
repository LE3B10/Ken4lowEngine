#define NOMINMAX
#include "SceneManager.h"

#include <ActorComponent.h>
#include <ActorWorld.h>
#include <SpriteManager.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <Editor/EditorCommandHistory.h>
#include <Editor/EditorContext.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif

#include <algorithm>
#include <cassert>
#include <vector>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	SceneManager::SceneManager() = default;
	SceneManager::~SceneManager() = default;

	void SceneManager::Initialize()
	{
		if (sceneTransition_) sceneTransition_->Initialize();
		isTransitioning_ = false;
		sceneSwapped_ = false;
		pendingCrack_ = false;
		coverHoldCounter_ = 0;
		uncoverDelayCounter_ = 0;
		unloadRequested_ = false;
		editorPlaySessionActive_ = false;
	}

	void SceneManager::UpdateEditorPlaySession()
	{
#ifdef USE_IMGUI
		if (!scene_ || K4E::EditorModeController::GetInstance()->IsGamePreviewMode()) return;
		K4E::EditorPlayController* playController = K4E::EditorPlayController::GetInstance();
		if (playController->IsPlaying() && !editorPlaySessionActive_)
		{
			scene_->BeginEditorPlay();
			editorPlaySessionActive_ = true;
		}
		else if (playController->IsEditing() && editorPlaySessionActive_)
		{
			scene_->EndEditorPlay();
			editorPlaySessionActive_ = false;
		}
#endif
	}

	void SceneManager::RefreshEditorVisualState(float deltaTime)
	{
#ifdef USE_IMGUI
		ActorWorld* actorWorld = scene_ ? scene_->GetEditorActorWorld() : nullptr;
		if (!actorWorld) return;
		for (const auto& actorOwner : actorWorld->GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || !actor->IsActive() || actor->IsPendingDestroy()) continue;
			std::vector<ActorComponent*> components;
			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (component && component->IsActiveInHierarchy()) components.push_back(component);
			}
			std::sort(components.begin(), components.end(), [](const ActorComponent* lhs, const ActorComponent* rhs)
				{
					return lhs->GetUpdateOrder() < rhs->GetUpdateOrder();
				});
			for (ActorComponent* component : components) component->UpdateEditor(deltaTime);
		}
#else
		(void)deltaTime;
#endif
	}

	void SceneManager::Update()
	{
		float dtRaw = K4E::GameTimer::GetInstance()->GetDeltaTime();
		float dtFade = std::min(dtRaw, 1.0f / 30.0f);
		if (sceneTransition_) sceneTransition_->Update(dtFade);

		if (isTransitioning_)
		{
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
					else coverHoldCounter_ = 0;
				}
				else coverHoldCounter_ = 0;
			}

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
				else uncoverDelayCounter_ = 0;
			}

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

		UpdateEditorPlaySession();
		if (scene_ && (!isTransitioning_ || sceneSwapped_))
		{
			bool shouldUpdateGame = true;
#ifdef USE_IMGUI
			shouldUpdateGame = K4E::EditorModeController::GetInstance()->IsGamePreviewMode() || K4E::EditorPlayController::GetInstance()->IsPlaying();
#endif
			if (shouldUpdateGame) scene_->Update();
			else
			{
				scene_->UpdateEditor(dtRaw);
				RefreshEditorVisualState(dtRaw);
			}
		}
	}

	void SceneManager::PrepareShadowPass() { if (scene_) scene_->PrepareShadowPass(); }
	void SceneManager::Draw3DObjects() { if (scene_) scene_->Draw3DObjects(); }
	void SceneManager::DrawShadowObjects() { if (scene_) scene_->DrawShadowObjects(); }

	void SceneManager::Draw2DSprites()
	{
		if (scene_) scene_->Draw2DSprites();
		if (sceneTransition_)
		{
			K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
			sceneTransition_->Draw2DSprites();
		}
	}

	void SceneManager::DrawImGui()
	{
		if (scene_) scene_->DrawImGui();
		if (sceneTransition_) sceneTransition_->DrawImGui();
	}

	void SceneManager::Finalize()
	{
		if (scene_)
		{
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				editorPlaySessionActive_ = false;
			}
			scene_->Finalize();
		}
		if (sceneTransition_) sceneTransition_->Finalize();
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
#endif
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
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
		K4E::EditorContext::GetInstance()->ResetTransientState(); // Scene固有ポインタを持つSelectionとCommandを同時に破棄する。
#endif
		if (scene_)
		{
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				editorPlaySessionActive_ = false;
			}
			scene_->Finalize();
		}
		scene_ = std::move(nextScene_);
		if (scene_)
		{
			scene_->SetSceneManager(this);
			scene_->Initialize();
			scene_->StartLoad();
		}
	}
} // namespace Ken4lowEngine
