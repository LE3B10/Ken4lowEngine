#include "GameFadeIn.h"
#include "GamePlayScene.h"
#include "GamePlayingState.h"

void GameFadeIn::Enter(GamePlayScene* scene)
{
	// シーンが有効か確認
	if (!scene) return;

	// シーンの状態をフェードイン中に設定
	scene->SetState(GamePlayScene::State::FadeIn);

	// タイマーリセット
	timer_ = 0.0f;

	// 真っ黒からスタート
	scene->SetFadeAlpha(1.0f);
}

void GameFadeIn::Update(GamePlayScene* scene, float deltaTime)
{
	if (!scene) return;

	timer_ += deltaTime;

	float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
	float alpha = 1.0f - t;   // 1 -> 0
	scene->SetFadeAlpha(alpha);

	if (t >= 1.0f)
	{
		// フェードイン完了 → プレイング状態へ
		scene->ChangeState(std::make_unique<GamePlayingState>());
	}
}

void GameFadeIn::Draw3DObjects(GamePlayScene* scene)
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

void GameFadeIn::Draw2DSprites(GamePlayScene* scene)
{
	if (!scene) return;

	auto* fadeSprite_ = scene->GetFadeSprite();
	float fadeAlpha_ = scene->GetFadeAlpha();

	// フェードオーバーレイ
	if (fadeSprite_ && fadeAlpha_ > 0.0f)
	{
		fadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, fadeAlpha_ });
		fadeSprite_->Update();
		fadeSprite_->Draw();
	}
}

void GameFadeIn::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}
