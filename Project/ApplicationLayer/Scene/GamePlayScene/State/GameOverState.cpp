#include "GameOverState.h"
#include "GamePlayScene.h"
#include "GameLoadState.h" // 遷移先ステート用(リトライ)
#include "SceneManager.h"

#include <DirectXCommon.h>

#include <Input.h>

void GameOverState::Enter(GamePlayScene* scene)
{
	if (!scene) return;
	using State = GamePlayScene::State;
	scene->SetState(State::GameOver);

	Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true);

	// --------- GameOver UI を中央にレイアウト（見えやすくする） ---------
	if (auto* dx = scene->GetDirectXCommon())
	{
		const float screenW = static_cast<float>(dx->GetSwapChainDesc().Width);
		const float screenH = static_cast<float>(dx->GetSwapChainDesc().Height);

		// 半透明パネル（既存の clearPanelSprite を流用）
		auto& panel = scene->GetClearPanelSprite();
		if (panel)
		{
			panel->SetAnchorPoint({ 0.5f, 0.5f });
			panel->SetPosition({ screenW * 0.5f, screenH * 0.52f });
			panel->SetSize({ screenW * 0.60f, screenH * 0.40f });
			panel->SetColor({ 0.0f, 0.0f, 0.0f, 0.65f });
		}

		// ボタン配置（中央・縦並び）
		const float btnW = 360.0f;
		const float btnH = 86.0f;
		const float gap = 18.0f;
		const float centerX = screenW * 0.5f;
		const float panelY = screenH * 0.52f;
		const float panelH = screenH * 0.40f;
		const float topY = panelY - panelH * 0.5f;

		auto& retrySprite = scene->GetRetryButtonSprite();
		auto& retireSprite = scene->GetRetireButtonSprite();
		auto& retryRect = scene->GetRetryRect();
		auto& retireRect = scene->GetRetireRect();

		const float retryY = topY + panelH * 0.55f;
		const float retireY = retryY + btnH + gap;

		if (retrySprite)
		{
			retrySprite->SetAnchorPoint({ 0.5f, 0.5f });
			retrySprite->SetSize({ btnW, btnH });
			retrySprite->SetPosition({ centerX, retryY });
			retrySprite->SetColor({ 0.2f, 1.0f, 0.2f, 0.85f });

			retryRect = { centerX - btnW * 0.5f, retryY - btnH * 0.5f, btnW, btnH };
		}

		if (retireSprite)
		{
			retireSprite->SetAnchorPoint({ 0.5f, 0.5f });
			retireSprite->SetSize({ btnW, btnH });
			retireSprite->SetPosition({ centerX, retireY });
			retireSprite->SetColor({ 1.0f, 0.2f, 0.2f, 0.85f });

			retireRect = { centerX - btnW * 0.5f, retireY - btnH * 0.5f, btnW, btnH };
		}
	}
}

void GameOverState::Update(GamePlayScene* scene, float deltaTime)
{
	// シーンが有効か確認
	if (!scene) return;

	using ButtonRect = GamePlayScene::ButtonRect;
	auto* input = scene->GetInput();
	auto* player = scene->GetPlayer();
	auto* skyBox = scene->GetSkyBox();
	auto* crosshair = scene->GetCrosshair();
	auto* itemManager = scene->GetItemManager();
	auto& levelObjectManager = scene->GetLevelObjectManager();
	auto& retryButtonSprite = scene->GetRetryButtonSprite();
	auto& retireButtonSprite = scene->GetRetireButtonSprite();
	auto& retryRect = scene->GetRetryRect();
	auto& retireRect = scene->GetRetireRect();

	auto& enemies = scene->GetEnemies();
	auto& boss = scene->GetBoss();

	// 死亡演出を最後まで回すためにプレイヤーだけは更新
	player->Update(deltaTime);

	// 背景など（敵が居れば更新）
	for (auto& e : enemies)
	{
		e->Update(deltaTime);
	}

	// ボス更新
	if (boss)
	{
		boss->Update(deltaTime);
	}

	skyBox->Update();
	crosshair->Update();
	itemManager->Update(player, deltaTime);
	retryButtonSprite->Update();
	retireButtonSprite->Update();
	levelObjectManager->Update();

	// --------- マウスクリック判定 ---------
	// マウス座標を取得（例：スクリーン座標のfloat2を返す想定）
	Vector2 mousePos = input->GetMousePosition(); // 想定API

	auto IsInside = [](const Vector2& p, const ButtonRect& r) {
		return (p.x >= r.x && p.x <= r.x + r.w &&
			p.y >= r.y && p.y <= r.y + r.h);
		};

	bool leftClick = input->TriggerMouse(0); // 左クリックが「今フレーム押した」

	if (leftClick)
	{
		// 右下(リトライ)
		if (IsInside(mousePos, retryRect))
		{
			SceneManager::GetInstance()->ChangeScene("GamePlayScene");
			return;
		}

		// 左下(リタイア/タイトルへ)
		if (IsInside(mousePos, retireRect))
		{
			SceneManager::GetInstance()->ChangeScene("TitleScene");
			return;
		}
	}
}

void GameOverState::Draw3DObjects(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;

	auto* player_ = scene->GetPlayer();
	auto& enemies_ = scene->GetEnemies();
	auto* itemManager_ = scene->GetItemManager();
	auto& levelObjectManager_ = scene->GetLevelObjectManager();
	auto& boss_ = scene->GetBoss();

	player_->Draw();

	for (auto& e : enemies_) {
		e->Draw();
	}

	if (boss_) {
		boss_->Draw();
	}

	itemManager_->Draw();

	levelObjectManager_->Draw();
}

void GameOverState::Draw2DSprites(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;

	auto& panelSprite_ = scene->GetClearPanelSprite();
	auto& retireButtonSprite_ = scene->GetRetireButtonSprite();
	auto& retryButtonSprite_ = scene->GetRetryButtonSprite();

	if (panelSprite_) panelSprite_->Draw();

	if (retireButtonSprite_) retireButtonSprite_->Draw();
	if (retryButtonSprite_)  retryButtonSprite_->Draw();
}

void GameOverState::Exit(GamePlayScene* scene)
{
	// パネルを元に戻す（次のステートに影響しないように）
	if (scene)
	{
		auto& panel = scene->GetClearPanelSprite();
		if (panel) { panel->SetColor({ 0.0f, 0.0f, 0.0f, 0.0f }); }
	}
}
