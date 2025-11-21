#define NOMINMAX
#include "StageSelectLoadState.h"
#include "StageSelectScene.h"
#include <StageSelectFadeInState.h>
#include <StageSelectFadeOutState.h>
#include "StageRepository.h"

void StageSelectLoadState::Enter(StageSelectScene* scene)
{
	if (!scene) { return; }

	scene->SetState(StageSelectScene::State::Loading);
	timer_ = 0.0f;

	// フェードアウト後なので、真っ黒を維持
	scene->SetFadeAlpha(1.0f);
}

void StageSelectLoadState::Update(StageSelectScene* scene, float deltaTime)
{
	if (!scene) { return; }

	timer_ += deltaTime;

	// ロード完了＆演出時間経過で遷移先へ
	if (timer_ >= duration_)
	{
		// NextScene を見て遷移先を決める
		switch (scene->GetNextScene())
		{
		case StageSelectScene::NextScene::Title:
			scene->BackToTitle();
			break;

		case StageSelectScene::NextScene::GamePlay:
			scene->GoToGamePlay();
			break;

		default:
			// ロードが終わったらフェードインステートへ
			scene->ChangeState(std::make_unique<StageSelectFadeInState>());
			auto* selector = scene->GetActiveSelector();
			selector->Update(deltaTime); // 念のため一回更新
			break;
		}
	}
}

void StageSelectLoadState::Exit(StageSelectScene* scene)
{
	(void)scene;
}
