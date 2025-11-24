#include "GameOverState.h"
#include "GamePlayScene.h"
#include "GameLoadState.h" // 遷移先ステート用(リトライ)
#include "SceneManager.h"

#include <Input.h>

void GameOverState::Enter(GamePlayScene* scene)
{
	if (!scene) return;
	using State = GamePlayScene::State;
	scene->SetState(State::GameOver);

	Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true);
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

void GameOverState::Exit(GamePlayScene* scene)
{
	// 特に何もしない
	(void)scene;
}
