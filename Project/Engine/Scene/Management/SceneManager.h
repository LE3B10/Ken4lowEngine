#pragma once

#include <BaseScene.h>
#include "AbstractSceneFactory.h"
#include "ISceneTransition.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// シーンの生成・切り替え・更新・描画を管理するクラス
	/// -------------------------------------------------------------
	class SceneManager
	{
	public:
		SceneManager();
		~SceneManager();

		void Initialize();
		void Update();
		void Draw3DObjects();
		void DrawShadowObjects();
		void Draw2DSprites();
		void DrawImGui();
		void Finalize();

		// 次のシーンの所有権をSceneManagerへ移す。
		void SetNextScene(std::unique_ptr<BaseScene> nextScene)
		{
			nextScene_ = std::move(nextScene);
		}

		// ゲーム固有のSceneFactoryをSceneManagerへ登録する。
		void SetAbstractSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory)
		{
			sceneFactory_ = std::move(sceneFactory);
		}

		// ゲーム固有のシーン遷移演出をSceneManagerへ登録する。
		void SetSceneTransition(std::unique_ptr<ISceneTransition> sceneTransition)
		{
			sceneTransition_ = std::move(sceneTransition);
		}

		void ChangeScene(const std::string& sceneName);

		[[nodiscard]] bool IsTransitioning() const
		{
			return isTransitioning_ || (sceneTransition_ && sceneTransition_->IsBusy());
		}

		[[nodiscard]] BaseScene* GetCurrentScene() { return scene_.get(); }
		[[nodiscard]] const BaseScene* GetCurrentScene() const { return scene_.get(); }

		// Editorは所有せず、現在登録されている遷移演出を参照する。
		[[nodiscard]] ISceneTransition* GetSceneTransition() { return sceneTransition_.get(); }
		[[nodiscard]] const ISceneTransition* GetSceneTransition() const { return sceneTransition_.get(); }

	private:
		void ApplyNextScene();

	private:
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
	};
}
