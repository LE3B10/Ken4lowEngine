#include "SceneFactory.h"

#include "TitleScene.h"
#include "GamePlayScene.h"
#include "StageSelectScene.h"
#include "DebugScene.h"

#include <stdexcept>
#include <utility>

namespace Ken4lowEngine
{
	SceneFactory::SceneFactory()
	{
		RegisterSceneClass("TitleScene", []() { return std::make_unique<TitleScene>(); });
		RegisterSceneClass("StageSelectScene", []() { return std::make_unique<StageSelectScene>(); });
		RegisterSceneClass("GamePlayScene", []() { return std::make_unique<GamePlayScene>(); });
#ifdef _DEBUG
		RegisterSceneClass("DebugScene", []() { return std::make_unique<DebugScene>(); });
#endif
	}

	void SceneFactory::RegisterSceneClass(std::string sceneClassName, SceneCreator creator)
	{
		if (sceneClassName.empty() || !creator) return;
		creators_.insert_or_assign(std::move(sceneClassName), std::move(creator)); // Scene追加時のif/else連鎖を登録表へ置き換える。
	}

	std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneClassName)
	{
		const auto creator = creators_.find(sceneClassName);
		if (creator == creators_.end())
		{
			throw std::runtime_error("Unknown scene class: " + sceneClassName);
		}
		return creator->second();
	}
} // namespace Ken4lowEngine
