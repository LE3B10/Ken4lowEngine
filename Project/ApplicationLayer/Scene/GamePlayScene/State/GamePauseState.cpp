#include "GamePauseState.h"
#include "GamePlayScene.h"
#include "GamePlayingState.h"
#include "PauseOverlay.h"
#include <SceneManager.h>
#include <Input.h>

#include <memory>

void GamePauseState::Enter(GamePlayScene* scene)
{
	if (!scene) return;

	using State = GamePlayScene::State;

	// 状態を Paused に
	scene->SetState(State::Paused);
	scene->SetPaused(true);

	// カーソル解放（ポーズメニューを操作できるように）
	Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true);
}

void GamePauseState::Update(GamePlayScene* scene, float deltaTime)
{
	(void)deltaTime;
	if (!scene) return;

	using State = GamePlayScene::State;

	auto* overlay = scene->GetPauseOverlay();
	auto* input = scene->GetInput();

	// ---------- ポーズオーバーレイの更新 ----------
	if (overlay)
	{
		overlay->Update();

		if (overlay->IsClose())
		{
			bool goTitle = overlay->IsGoTitle();

			// オーバーレイを閉じる
			scene->ClearPauseOverlay();
			scene->SetPaused(false);

			if (goTitle)
			{
				// タイトルシーンへ

				//SceneManager::GetInstance()->ChangeScene("TitleScene");
				return;
			}
			else
			{
				// ゲーム再開（Playing へ戻る）
				Input::GetInstance()->SetLockCursor(true);
				ShowCursor(false);

				scene->SetState(State::Playing);
				scene->ChangeState(std::make_unique<GamePlayingState>());
				return;
			}
		}
	}

	// ---------- ESC キーでクイック解除 ----------
	if (input->TriggerKey(DIK_ESCAPE))
	{
		// ポーズメニューを閉じてそのままゲーム再開
		scene->ClearPauseOverlay();
		scene->SetPaused(false);

		Input::GetInstance()->SetLockCursor(true);
		ShowCursor(false);

		scene->SetState(State::Playing);
		scene->ChangeState(std::make_unique<GamePlayingState>());
		return;
	}
}

void GamePauseState::Draw3DObjects(GamePlayScene* scene)
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

void GamePauseState::Draw2DSprites(GamePlayScene* scene)
{
	if (!scene) return;

	auto* pauseOverlay_ = scene->GetPauseOverlay();

	// ---------- ポーズオーバーレイ ----------
	if (pauseOverlay_) pauseOverlay_->Draw2D();
}

void GamePauseState::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}
