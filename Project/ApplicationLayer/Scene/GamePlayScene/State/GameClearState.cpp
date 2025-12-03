#include "GameClearState.h"
#include <GamePlayScene.h>
#include "SceneManager.h"

#include <Input.h>
#include <StageRepository.h>
#include <Player.h>

void GameClearState::Enter(GamePlayScene* scene)
{
	if (!scene) return;

	scene->SetState(GamePlayScene::State::GameClear);

	Input::GetInstance()->SetLockCursor(false);
	ShowCursor(true);
}

void GameClearState::Update(GamePlayScene* scene, float deltaTime)
{
	// シーンが有効か確認
	if (!scene) return;

	using State = GamePlayScene::State;
	using ButtonRect = GamePlayScene::ButtonRect;

	auto* input_ = scene->GetInput();
	auto* player_ = scene->GetPlayer();
	auto& boss = scene->GetBoss();
	auto* skyBox_ = scene->GetSkyBox();
	auto* itemManager_ = scene->GetItemManager();
	auto& clearPanelSprite_ = scene->GetClearPanelSprite();
	auto& clearTextSprite_ = scene->GetClearTextSprite();
	auto& clearStarSprites_ = scene->GetClearStarSprites();
	auto& clearStarDelay_ = scene->GetClearStarDelay();
	auto& clearStarBurstPlayed_ = scene->GetClearStarBurstPlayed();
	auto& retireButtonSprite_ = scene->GetRetireButtonSprite();
	auto& retryButtonSprite_ = scene->GetRetryButtonSprite();
	auto& clearOptionSprites_ = scene->GetClearOptionSprites();
	auto& clearOptionRects_ = scene->GetClearOptionRects();
	auto& levelObjectManager = scene->GetLevelObjectManager();

	int kClearStarCount = scene->GetClearStarCount();
	int currentStageIndex_ = scene->GetCurrentStageIndex();

	bool gameClearInputAccepted_ = scene->IsGameClearInputAccepted();
	float gameClearTimer_ = scene->GetGameClearTimer();
	float clearStarPopDuration_ = scene->GetClearStarPopDuration();
	float clearStarBaseSize_ = scene->GetClearStarBaseSize();

	// 背景やアイテムの軽い更新
	skyBox_->Update();
	itemManager_->Update(player_, deltaTime);
	boss->Update(deltaTime);
	levelObjectManager->Update();

	// タイマー進行
	gameClearTimer_ += deltaTime;

	// =========================================================
	// ① パネル＆テキストのフェードイン
	// =========================================================
	const float panelFadeTime = 0.5f;
	float panelT = std::clamp(gameClearTimer_ / panelFadeTime, 0.0f, 1.0f);

	if (clearPanelSprite_)
	{
		auto col = clearPanelSprite_->GetColor();
		col.w = panelT * 0.8f;  // 少し透けた黒
		clearPanelSprite_->SetColor(col);
		clearPanelSprite_->Update();
	}

	if (clearTextSprite_)
	{
		auto col = clearTextSprite_->GetColor();
		col.w = panelT;
		clearTextSprite_->SetColor(col);
		clearTextSprite_->Update();
	}

	// =========================================================
	// ② 星３つのポップ演出
	// =========================================================
	bool allStarsFinished = true;

	for (int i = 0; i < kClearStarCount; ++i)
	{
		if (!clearStarSprites_[i]) continue;

		float local = (gameClearTimer_ - clearStarDelay_[i]) / clearStarPopDuration_;

		if (local <= 0.0f)
		{
			// まだ出番が来ていない星
			allStarsFinished = false;
			continue;
		}

		float u = std::clamp(local, 0.0f, 1.0f);

		// サイズを0→基準サイズに補間
		float size = clearStarBaseSize_ * u;
		clearStarSprites_[i]->SetSize({ size, size });

		// アルファも0→1
		auto col = clearStarSprites_[i]->GetColor();
		col.w = u;
		clearStarSprites_[i]->SetColor(col);
		clearStarSprites_[i]->Update();

		// ちょうど出始めのフレームで一回だけ花火パーティクルを出す想定
		if (!clearStarBurstPlayed_[i])
		{
			if (local >= 0.0f) // 0を跨いだ瞬間
			{
				// ★ここに自分のパーティクル呼び出しを入れる
				//   例）SpriteParticleManager::GetInstance()->EmitFirework(
				//           clearStarSprites_[i]->GetPosition());
				clearStarBurstPlayed_[i] = true;
			}
		}

		if (u < 1.0f)
		{
			// まだアニメ途中の星がある
			allStarsFinished = false;
		}
	}

	// ボタンも軽くアップデート
	if (retireButtonSprite_) retireButtonSprite_->Update();
	if (retryButtonSprite_)  retryButtonSprite_->Update();
	for (auto& s : clearOptionSprites_) {
		if (s) s->Update();
	}

	// =========================================================
	// ③ 全部の星が出そろったら入力受付開始
	// =========================================================
	// 星のポップが全部終わったら入力受付開始
	if (!gameClearInputAccepted_ && allStarsFinished)
	{
		gameClearInputAccepted_ = true;
	}

	if (gameClearInputAccepted_)
	{
		// --- マウス操作（三つの長方形で三択） ---
		Vector2 mousePos = input_->GetMousePosition();
		auto IsInside = [](const Vector2& p, const ButtonRect& r) {
			return (p.x >= r.x && p.x <= r.x + r.w &&
				p.y >= r.y && p.y <= r.y + r.h);
			};
		bool leftClick = input_->TriggerMouse(0);

		if (leftClick)
		{
			// 0: 左ボタン → もう一度同じステージを遊ぶ
			if (IsInside(mousePos, clearOptionRects_[0]))
			{
				auto& repo = StageRepository::GetInstance();
				// currentStageIndex_ は今のステージ番号を持っている前提
				repo.SetStartIndex(currentStageIndex_);
				SceneManager::GetInstance()->ChangeScene("GamePlayScene");
				return;
			}

			// 1: 真ん中ボタン → 次のステージへ進む
			if (IsInside(mousePos, clearOptionRects_[1]))
			{
				// OnStageClear() 内で repo.SetStartIndex(next) 済みのはずなので、
				// そのまま GamePlayScene をロードすると「次ステージ」が始まる
				SceneManager::GetInstance()->ChangeScene("GamePlayScene");
				return;
			}

			// 2: 右ボタン → セレクトシーンに戻る
			if (IsInside(mousePos, clearOptionRects_[2]))
			{
				SceneManager::GetInstance()->ChangeScene("StageSelectScene");
				return;
			}
		}

		// キーボードで簡易操作：Enter / Space は「セレクトへ戻る」にしておく
		if (input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE))
		{
			SceneManager::GetInstance()->ChangeScene("StageSelectScene");
			return;
		}
	}

	// シーンに書き戻す
	scene->SetGameClearTimer(gameClearTimer_);
	scene->SetGameClearInputAccepted(gameClearInputAccepted_);

}

void GameClearState::Draw3DObjects(GamePlayScene* scene)
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

void GameClearState::Draw2DSprites(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;

	auto& clearPanelSprite_ = scene->GetClearPanelSprite();
	auto& clearTextSprite_ = scene->GetClearTextSprite();
	auto& clearStarSprites_ = scene->GetClearStarSprites();
	auto& clearOptionSprites_ = scene->GetClearOptionSprites();

	if (clearPanelSprite_) clearPanelSprite_->Draw();
	if (clearTextSprite_)  clearTextSprite_->Draw();

	// 星（飾り）：そのまま描画
	for (auto& s : clearStarSprites_) {
		if (s) s->Draw();
	}

	// ★ 三択ボタン
	for (auto& s : clearOptionSprites_) {
		if (s) s->Draw();
	}
}

void GameClearState::Exit(GamePlayScene* scene)
{
	// 特にやることなし
	(void)scene; // 未使用
}
