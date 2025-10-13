#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
#include "GameClearScene.h"
#include "GameOverScene.h"
#include "StageSelectScene.h"
#include "WorldMapScene.h"
#include <PhysicalScene.h>


/// -------------------------------------------------------------
///				　		    シーン生成
/// -------------------------------------------------------------
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	// 次のシーンを生成
	std::unique_ptr<BaseScene> newScene = nullptr;

	// タイトルシーン
	if (sceneName == "TitleScene")				return std::make_unique<TitleScene>();

	// ステージセレクトシーン
	else if (sceneName == "StageSelectScene")	return std::make_unique<StageSelectScene>();

	// ワールドマップシーン
	else if (sceneName == "WorldMapScene")		return std::make_unique<WorldMapScene>();

	// ゲームプレイシーン
	else if (sceneName == "GamePlayScene")		return std::make_unique<GamePlayScene>();

	// ゲームクリアシーン
	else if (sceneName == "GameClearScene")		return std::make_unique<GameClearScene>();

	// ゲームオーバーシーン
	else if (sceneName == "GameOverScene")		return std::make_unique<GameOverScene>();

#ifdef _DEBUG
	// 物理演算シーン
	else if (sceneName == "PhysicalScene")		return std::make_unique<PhysicalScene>();
#endif // _DEBUG

	throw std::runtime_error("Unknown scene name: " + sceneName);
}
