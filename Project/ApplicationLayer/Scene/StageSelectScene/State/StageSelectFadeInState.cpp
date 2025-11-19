#define NOMINMAX
#include "StageSelectFadeInState.h"
#include "StageSelectScene.h"
#include "StageSelectSelectingState.h"

#include <algorithm>

void StageSelectFadeInState::Enter(StageSelectScene* scene)
{
	if (!scene) { return; }

	scene->SetState(StageSelectScene::State::FadingIn);
	timer_ = 0.0f;

	// 真っ黒からスタート
	scene->SetFadeAlpha(1.0f);
}

void StageSelectFadeInState::Update(StageSelectScene* scene, float deltaTime)
{
	if (!scene) { return; }

	timer_ += deltaTime;

	float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
	float alpha = 1.0f - t;   // 1 -> 0
	scene->SetFadeAlpha(alpha);

	if (t >= 1.0f)
	{
		// フェードイン完了 → セレクト状態へ
		scene->ChangeState(std::make_unique<StageSelectSelectingState>());
	}
}

void StageSelectFadeInState::Exit(StageSelectScene* scene)
{
	(void)scene; // 未使用
}
