#define NOMINMAX
#include "StageSelectSelectingState.h"
#include "StageSelectScene.h"

#include <DirectXCommon.h>
#include <Input.h>
#include "GridStageSelector.h"
#include "StageRepository.h"
#include "LinearInterpolation.h"
#include <StageSelectFadeOutState.h>

#include <algorithm>

void StageSelectSelectingState::Enter(StageSelectScene* scene)
{
	if (!scene) { return; }

	// 念のため状態を初期化しておく
	scene->SetState(StageSelectScene::State::Selecting);
}

void StageSelectSelectingState::Update(StageSelectScene* scene, float deltaTime)
{
	if (!scene) { return; }

	auto* dxCommon = scene->GetDxCommon();
	auto* input = scene->GetInput();
	if (!dxCommon || !input) { return; }

	// --- セレクタ更新 ---
	if (auto* selector = scene->GetActiveSelector())
	{
		selector->Update(deltaTime);
	}

	// --- 背景色の補間更新 ---
	auto& bgNow = scene->GetBgNow();
	auto& bgTarget = scene->GetBgTarget();

	float t = std::clamp(deltaTime * 4.0f, 0.0f, 1.0f);
	bgNow = Lerp(bgNow, bgTarget, t);
	bgNow.w = 1.0f; // 念のため

	if (auto* bgSprite = scene->GetBgSprite())
	{
		bgSprite->SetColor(bgNow);
		bgSprite->Update();
	}

	// --- ESC でタイトルへ戻る ---
	if (input->TriggerKey(DIK_ESCAPE))
	{
		scene->SetNextScene(StageSelectScene::NextScene::Title);
		scene->ChangeState(std::make_unique<StageSelectFadeOutState>());
		return;
	}

	// --- アンロック演出（justUnlocked フラグの処理） ---
	int& pendingIndex = scene->GetPendingUnlockIndex();
	if (pendingIndex >= 0)
	{
		auto* selector = scene->GetActiveSelector();

		if (auto* grid = dynamic_cast<GridStageSelector*>(selector))
		{
			grid->PlayUnlockAnim(pendingIndex);
		}
		else if (selector)
		{
			// Grid 以外でも最低限フォーカスだけは合わせる
			selector->FocusToIndex(pendingIndex, false);
		}

		auto& stages = scene->GetStages();
		if (pendingIndex >= 0 && pendingIndex < static_cast<int>(stages.size()))
		{
			stages[pendingIndex].justUnlocked = false;
			StageRepository::GetInstance().SetStages(stages);
		}

		pendingIndex = -1; // 一度きり
	}
}

void StageSelectSelectingState::Exit(StageSelectScene* scene)
{
	// 特にやることなし
	(void)scene; // 未使用パラメータ抑制
}
