#include "TitleFadeOutState.h"
#include "TitleScene.h"
#include "SceneManager.h"
#include "TitleLoadState.h"

#include <algorithm> // std::clamp 用

void TitleFadeOutState::Enter(TitleScene* scene)
{
	// ステート名更新
	scene->SetState(TitleScene::State::FadeOut);

	// タイマー初期化
	timer_ = 0.0f;

	// フェード開始時は透明
	scene->SetFadeAlpha(0.0f);
}

void TitleFadeOutState::Update(TitleScene* scene, float deltaTime)
{
	timer_ += deltaTime;
	float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);

	// α を 0→1 に上げる
	scene->SetFadeAlpha(t);

	// 完全に黒くなったらロード状態へ
	if (t >= 1.0f)
	{
		scene->ChangeState(std::make_unique<TitleLoadState>());
	}
}

void TitleFadeOutState::Exit(TitleScene* scene)
{
	(void)scene; // 未使用引数対策
}
