#pragma once

#include <BaseScene.h>
#include "AbstractSceneFactory.h"
#include "ISceneTransition.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	class SceneManager
	{
	public:
		SceneManager();
		~SceneManager();

		void Initialize();
		void ProcessEditorPlayRequests();
		void Update();
		void PrepareShadowPass();
		void Draw3DObjects();
		void DrawShadowObjects();
		void Draw2DSprites();
		void DrawImGui();
		void Finalize();

		void SetNextScene(std::unique_ptr<BaseScene> nextScene) { nextScene_ = std::move(nextScene); }
		void SetAbstractSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory) { sceneFactory_ = std::move(sceneFactory); }
		void SetSceneTransition(std::unique_ptr<ISceneTransition> sceneTransition) { sceneTransition_ = std::move(sceneTransition); }
		void ChangeScene(const std::string& sceneName);

		[[nodiscard]] bool IsTransitioning() const { return isTransitioning_ || (sceneTransition_ && sceneTransition_->IsBusy()); }
		[[nodiscard]] bool IsPlayInEditorActive() const { return editorPlaySessionActive_; }
		[[nodiscard]] BaseScene* GetCurrentScene() { return scene_.get(); }
		[[nodiscard]] const BaseScene* GetCurrentScene() const { return scene_.get(); }
		[[nodiscard]] ISceneTransition* GetSceneTransition() { return sceneTransition_.get(); }
		[[nodiscard]] const ISceneTransition* GetSceneTransition() const { return sceneTransition_.get(); }

	private:
		void ApplyNextScene();
		void RefreshEditorVisualState(float deltaTime);

		std::unique_ptr<BaseScene> scene_;
		std::unique_ptr<BaseScene> nextScene_;
		std::unique_ptr<AbstractSceneFactory> sceneFactory_;
		std::unique_ptr<ISceneTransition> sceneTransition_;

		bool isTransitioning_ = false;
		bool sceneSwapped_ = false;
		bool pendingCrack_ = false;
		bool hasQueuedChange_ = false;
		std::string queuedSceneName_;
		int coverHoldFrames_ = 4;
		int coverHoldCounter_ = 0;
		int uncoverDelayFrames_ = 3;
		int uncoverDelayCounter_ = 0;
		bool unloadRequested_ = false;
		bool editorPlaySessionActive_ = false; // Runtime Worldが存在する期間はPause中もtrueを維持する。
		bool editorSingleStepRequested_ = false; // Pause中の1フレーム実行を通常Playと分離する。
	};
} // namespace Ken4lowEngine
