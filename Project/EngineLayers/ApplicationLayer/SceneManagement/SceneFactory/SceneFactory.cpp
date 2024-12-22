#include "SceneFactory.h"
#include "TitleScene.h"  // TitleScene をインクルード
#include "GamePlayScene.h" // GamePlayScene をインクルード


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

    throw std::runtime_error("Unknown scene name: " + sceneName);
}
