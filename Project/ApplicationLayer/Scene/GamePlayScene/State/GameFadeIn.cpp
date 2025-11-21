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

void GameFadeIn::Exit(GamePlayScene* scene)
{
	(void)scene; // 未使用
}
