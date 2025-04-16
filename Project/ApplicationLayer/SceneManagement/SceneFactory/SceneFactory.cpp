#include "SceneFactory.h"
#include "TitleScene.h"  // TitleScene をインクルード
#include "GamePlayScene.h" // GamePlayScene をインクルード
#include "GameClearScene.h"
#include "GameOverScene.h"


/// -------------------------------------------------------------
///				　		    シーン生成
/// -------------------------------------------------------------
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
    // 次のシーンを生成
    std::unique_ptr<BaseScene> newScene = nullptr;

    if (sceneName == "TitleScene")
    {
        return std::make_unique<TitleScene>();
    }
    else if (sceneName == "GamePlayScene")
    {
        return std::make_unique<GamePlayScene>();
    }
    else if (sceneName == "GameClearScene")
    {
        return std::make_unique<GameClearScene>();
    }
    else if (sceneName == "GameOverScene")
    {
        return std::make_unique<GameOverScene>();
    }

    throw std::runtime_error("Unknown scene name: " + sceneName);
}
