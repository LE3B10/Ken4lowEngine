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
#include <Editor/EditorPlaySessionManager.h>
#include <Editor/EditorWindowManager.h>
#endif

#include <algorithm>
#include <cassert>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
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
		editorSingleStepRequested_ = false;
		currentSceneDefinition_.reset();
		nextSceneDefinition_.reset();
	}

	bool SceneManager::LoadSceneDefinitions(const std::filesystem::path& directory)
	{
		const bool loaded = sceneRegistry_.LoadDirectory(directory);
		for (const std::string& warning : sceneRegistry_.GetWarnings())
		{
			ReportSceneMessage(true, "[SceneRegistry][Warning] " + warning);
		}
		for (const std::string& error : sceneRegistry_.GetErrors())
		{
			ReportSceneMessage(false, "[SceneRegistry] " + error);
		}
		if (loaded)
		{
			ReportSceneMessage(true, "[SceneRegistry] Loaded " +
				std::to_string(sceneRegistry_.GetDefinitions().size()) + " scene definitions from " + directory.generic_string());
		}
		return loaded;
	}

	std::string SceneManager::GetConfiguredStartSceneName(bool debugBuild, std::string_view fallback) const
	{
		return sceneRegistry_.GetStartupSceneName(debugBuild, fallback);
	}

	SceneDefinition SceneManager::ResolveSceneDefinition(std::string_view sceneName) const
	{
		if (const SceneDefinition* definition = sceneRegistry_.Find(sceneName)) return *definition;
		return SceneRegistry::MakeLegacyDefinition(sceneName); // 未移行の呼び出しは従来どおりSceneClass名として扱う。
	}

	void SceneManager::ApplyTransitionDefinition(const SceneDefinition& definition)
	{
		coverHoldFrames_ = (std::max)(0, definition.transition.coverHoldFrames);
		uncoverDelayFrames_ = (std::max)(0, definition.transition.uncoverDelayFrames);
		// Transitionの具体クラスはGameApplicationから注入し、Scene JSONは切り替えタイミングだけを所有する。
	}

	void SceneManager::ReportSceneMessage(bool succeeded, const std::string& message) const
	{
#ifdef USE_IMGUI
		K4E::EditorWindowManager::GetInstance()->AddOutputLog(
			succeeded ? K4E::EditorLogLevel::Info : K4E::EditorLogLevel::Error,
			message);
#else
		std::clog << message << '\n';
#endif
	}

	void SceneManager::ProcessEditorPlayRequests()
	{
#ifdef USE_IMGUI
		if (!scene_) return;

		K4E::EditorPlayController* playController = K4E::EditorPlayController::GetInstance();
		K4E::EditorPlaySessionManager* sessionManager = K4E::EditorPlaySessionManager::GetInstance();
		const K4E::EditorPlayRequest request = playController->ConsumePendingRequest();

		switch (request)
		{
		case K4E::EditorPlayRequest::Start:
			if (!editorPlaySessionActive_ && sessionManager->BeginPlaySession(*scene_))
			{
				scene_->BeginEditorPlay();
				editorPlaySessionActive_ = true;
				editorSingleStepRequested_ = false;
				playController->CommitPlayStarted();
			}
			else
			{
				playController->CommitStopped();
			}
			break;

		case K4E::EditorPlayRequest::Resume:
			if (editorPlaySessionActive_)
			{
				playController->CommitPlayResumed();
			}
			else
			{
				playController->CommitStopped();
			}
			break;

		case K4E::EditorPlayRequest::Pause:
			if (editorPlaySessionActive_) playController->CommitPaused();
			break;

		case K4E::EditorPlayRequest::Step:
			if (editorPlaySessionActive_)
			{
				playController->CommitPaused();
				editorSingleStepRequested_ = true; // Pause状態を保ったままこのUpdateだけRuntime Tickを許可する。
			}
			break;

		case K4E::EditorPlayRequest::Stop:
		case K4E::EditorPlayRequest::KeepChangesAndStop:
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				const bool keepChanges = request == K4E::EditorPlayRequest::KeepChangesAndStop;
				if (sessionManager->EndPlaySession(*scene_, keepChanges))
				{
					editorPlaySessionActive_ = false;
					editorSingleStepRequested_ = false;
					playController->CommitStopped();
				}
				else
				{
					playController->CommitPaused(); // 復元失敗時はRuntimeを進めず再操作できる状態で止める。
				}
			}
			else
			{
				playController->CommitStopped();
			}
			break;

		case K4E::EditorPlayRequest::None:
		default:
			break;
		}

		std::string statusMessage;
		bool statusSucceeded = false;
		if (sessionManager->ConsumeStatus(statusMessage, statusSucceeded))
		{
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(
				statusSucceeded ? K4E::EditorLogLevel::Info : K4E::EditorLogLevel::Error,
				statusMessage);
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
		float dtFade = (std::min)(dtRaw, 1.0f / 30.0f);
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

		if (scene_ && (!isTransitioning_ || sceneSwapped_))
		{
			bool shouldUpdateGame = true;
#ifdef USE_IMGUI
			K4E::EditorPlayController* playController = K4E::EditorPlayController::GetInstance();
			shouldUpdateGame = K4E::EditorModeController::GetInstance()->IsGamePreviewMode() ||
				playController->IsPlaying() || editorSingleStepRequested_;
#endif
			if (shouldUpdateGame)
			{
				scene_->Update();
#ifdef USE_IMGUI
				if (editorPlaySessionActive_)
				{
					K4E::EditorPlaySessionManager::GetInstance()->NotifyRuntimeTick(dtRaw, *scene_);
				}
#endif
			}
			else
			{
				scene_->UpdateEditor(dtRaw);
				RefreshEditorVisualState(dtRaw);
			}
		}
		editorSingleStepRequested_ = false;
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
#ifdef USE_IMGUI
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				K4E::EditorPlaySessionManager::GetInstance()->CancelSessionWithoutRestore();
				editorPlaySessionActive_ = false;
				K4E::EditorPlayController::GetInstance()->CommitStopped();
			}
#endif
			scene_->Finalize();
		}
		if (sceneTransition_) sceneTransition_->Finalize();
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
#endif
		currentSceneDefinition_.reset();
		nextSceneDefinition_.reset();
	}

	void SceneManager::ChangeScene(const std::string& sceneName)
	{
		assert(sceneFactory_);
#ifdef USE_IMGUI
		if (editorPlaySessionActive_ || K4E::EditorPlaySessionManager::GetInstance()->IsSessionActive())
		{
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(
				K4E::EditorLogLevel::Warning,
				"PIE中のScene切り替えはEditor Worldを保護するため無効です。Stop後に切り替えてください。");
			return; // 現段階では元SceneのEditor Worldを確実に復元することを優先する。
		}
#endif
		if (IsTransitioning())
		{
			queuedSceneName_ = sceneName;
			hasQueuedChange_ = true;
			return;
		}

		SceneDefinition definition = ResolveSceneDefinition(sceneName);
		try
		{
			nextScene_ = sceneFactory_->CreateScene(definition.sceneClass);
		}
		catch (const std::exception& exception)
		{
			nextScene_.reset();
			ReportSceneMessage(false, "[SceneManager] Failed to create " + definition.sceneName +
				" (" + definition.sceneClass + "): " + exception.what());
			return;
		}
		if (!nextScene_)
		{
			ReportSceneMessage(false, "[SceneManager] SceneFactory returned null: " + definition.sceneClass);
			return;
		}

		nextSceneDefinition_ = std::move(definition);
		ApplyTransitionDefinition(*nextSceneDefinition_);
		ReportSceneMessage(true, "[SceneManager] ChangeScene: " + nextSceneDefinition_->sceneName +
			" -> " + nextSceneDefinition_->sceneClass);

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

	void SceneManager::ChangeToNextScene()
	{
		if (!currentSceneDefinition_ || currentSceneDefinition_->nextScene.empty())
		{
			ReportSceneMessage(false, "[SceneManager] NextScene is not configured for the current scene.");
			return;
		}
		ChangeScene(currentSceneDefinition_->nextScene);
	}

	void SceneManager::RestartCurrentScene()
	{
		if (!currentSceneDefinition_)
		{
			ReportSceneMessage(false, "[SceneManager] Current scene definition is not available.");
			return;
		}
		const std::string target = currentSceneDefinition_->retryScene.empty()
			? currentSceneDefinition_->sceneName
			: currentSceneDefinition_->retryScene;
		ChangeScene(target);
	}

	void SceneManager::ApplyNextScene()
	{
		if (!nextScene_) return;
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
		K4E::EditorContext::GetInstance()->ResetTransientState();
#endif
		if (scene_)
		{
#ifdef USE_IMGUI
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				K4E::EditorPlaySessionManager::GetInstance()->CancelSessionWithoutRestore();
				editorPlaySessionActive_ = false;
				K4E::EditorPlayController::GetInstance()->CommitStopped();
			}
#endif
			scene_->Finalize();
		}

		scene_ = std::move(nextScene_);
		currentSceneDefinition_ = nextSceneDefinition_.value_or(SceneRegistry::MakeLegacyDefinition("InjectedScene"));
		nextSceneDefinition_.reset();
		if (scene_)
		{
			scene_->SetSceneManager(this);
			scene_->SetSceneDefinition(*currentSceneDefinition_);
			scene_->Initialize();
			scene_->StartLoad();
		}
	}
} // namespace Ken4lowEngine
