#define NOMINMAX
#include "StageSelectFadeOutState.h"
#include "StageSelectScene.h"
#include "StageSelectLoadState.h"

#include <algorithm>

void StageSelectFadeOutState::Enter(StageSelectScene* scene)
{
    if (!scene) { return; }

    scene->SetState(StageSelectScene::State::FadingOut);
    timer_ = 0.0f;

    // 透明からスタート
    scene->SetFadeAlpha(0.0f);
}

void StageSelectFadeOutState::Update(StageSelectScene* scene, float deltaTime)
{
    if (!scene) { return; }

    timer_ += deltaTime;

    float t = std::clamp(timer_ / duration_, 0.0f, 1.0f);
    scene->SetFadeAlpha(t);   // 0 -> 1

    if (t >= 1.0f)
    {
        // フェードアウト完了 → ロード状態へ
        scene->ChangeState(std::make_unique<StageSelectLoadState>());
    }
}

void StageSelectFadeOutState::Exit(StageSelectScene* scene)
{
    (void)scene;
}