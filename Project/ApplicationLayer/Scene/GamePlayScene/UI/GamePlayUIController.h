#pragma once

#include "FadeManager.h"
#include "GamePlayFlow.h"
#include "GamePlayWorld.h"

#include <SpriteManager.h>

/// <summary>
/// GamePlayScene の 2D UI 描画順を管理する軽量 Controller です。<br/>
/// Scene 本体から HUD / Flow UI / Fade の描画詳細を切り離し、
/// Scene は「いつ描くか」だけを決められるようにします。
/// </summary>
class GamePlayUIController
{
public:
	/// <summary>
	/// HUD、ポーズ/リザルトUI、Scene内フェードを既存順序のまま描画します。
	/// </summary>
	void Draw2DSprites(GamePlayWorld* world, GamePlayFlow* flow, FadeManager* fadeManager, bool hideGameplayUI)
	{
		Ken4lowEngine::SpriteManager::GetInstance()->SetRenderSetting_Background();
		Ken4lowEngine::SpriteManager::GetInstance()->SetRenderSetting_UI();

		if (world)
		{
			// HUD は World が所有するため、表示抑制条件だけ Scene 側から渡す。
			world->DrawHUD(hideGameplayUI);
		}

		if (flow && !hideGameplayUI)
		{
			flow->DrawUI();
		}

		if (fadeManager)
		{
			fadeManager->Draw2DSprites();
		}
	}
};
