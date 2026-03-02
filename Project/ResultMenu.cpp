#include "ResultMenu.h"
#include "DirectXCommon.h"
#include "Input.h"

using namespace Ken4lowEngine;

void ResultMenu::Initialize()
{
	const float screenW = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float screenH = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	const float centerX = screenW * 0.5f;
	const float centerY = screenH * 0.5f;

	// ----------------------------------------
	// 背景暗転
	// ----------------------------------------
	backdrop_ = std::make_unique<Sprite>();
	backdrop_->Initialize("result_ui/result_backdrop.png");
	backdrop_->SetAnchorPoint({ 0.5f, 0.5f });
	backdrop_->SetPosition({ centerX, centerY });
	backdrop_->SetSize({ screenW, screenH });
	backdrop_->SetColor({ 1.0f, 1.0f, 1.0f, 0.65f });

	// ----------------------------------------
	// ヘッダー
	// ----------------------------------------
	clearHeader_ = std::make_unique<Sprite>();
	clearHeader_->Initialize("result_ui/result_stage_clear.png");
	clearHeader_->SetAnchorPoint({ 0.5f, 0.5f });
	clearHeader_->SetPosition({ centerX, centerY - 170.0f });
	clearHeader_->SetSize({ 540.0f, 132.0f });

	gameOverHeader_ = std::make_unique<Sprite>();
	gameOverHeader_->Initialize("result_ui/result_game_over.png");
	gameOverHeader_->SetAnchorPoint({ 0.5f, 0.5f });
	gameOverHeader_->SetPosition({ centerX, centerY - 170.0f });
	gameOverHeader_->SetSize({ 540.0f, 132.0f });

	// ----------------------------------------
	// ボタン
	// ----------------------------------------
	nextStageButton_.sprite = std::make_unique<Sprite>();
	nextStageButton_.sprite->Initialize("result_ui/result_next_stage.png");
	nextStageButton_.basePosition = { centerX, centerY - 20.0f };
	nextStageButton_.baseSize = { 360.0f, 84.0f };
	nextStageButton_.sprite->SetAnchorPoint({ 0.5f, 0.5f });
	nextStageButton_.sprite->SetPosition(nextStageButton_.basePosition);
	nextStageButton_.sprite->SetSize(nextStageButton_.baseSize);

	retryButton_.sprite = std::make_unique<Sprite>();
	retryButton_.sprite->Initialize("result_ui/result_retry.png");
	retryButton_.basePosition = { centerX, centerY + 82.0f };
	retryButton_.baseSize = { 360.0f, 84.0f };
	retryButton_.sprite->SetAnchorPoint({ 0.5f, 0.5f });
	retryButton_.sprite->SetPosition(retryButton_.basePosition);
	retryButton_.sprite->SetSize(retryButton_.baseSize);

	titleButton_.sprite = std::make_unique<Sprite>();
	titleButton_.sprite->Initialize("result_ui/result_title.png");
	titleButton_.basePosition = { centerX, centerY + 184.0f };
	titleButton_.baseSize = { 360.0f, 84.0f };
	titleButton_.sprite->SetAnchorPoint({ 0.5f, 0.5f });
	titleButton_.sprite->SetPosition(titleButton_.basePosition);
	titleButton_.sprite->SetSize(titleButton_.baseSize);

	isOpen_ = false;
}

void ResultMenu::Update()
{
	backdrop_->Update();
	clearHeader_->Update();
	gameOverHeader_->Update();
	nextStageButton_.sprite->Update();
	retryButton_.sprite->Update();
	titleButton_.sprite->Update();
}

void ResultMenu::Open(ResultMenuMode mode)
{
	mode_ = mode;
	isOpen_ = true;
}

void ResultMenu::Close()
{
	isOpen_ = false;
}

bool ResultMenu::IsMouseInside(const Vector2& mousePos,
	const Vector2& center,
	const Vector2& size) const
{
	const float left = center.x - size.x * 0.5f;
	const float right = center.x + size.x * 0.5f;
	const float top = center.y - size.y * 0.5f;
	const float bottom = center.y + size.y * 0.5f;

	return (mousePos.x >= left && mousePos.x <= right &&
		mousePos.y >= top && mousePos.y <= bottom);
}

void ResultMenu::UpdateButtonVisual(Button& button, bool hovered)
{
	const float scale = hovered ? 1.08f : 1.0f;
	const float alpha = hovered ? 1.0f : 0.9f;

	button.sprite->SetPosition(button.basePosition);
	button.sprite->SetSize({
		button.baseSize.x * scale,
		button.baseSize.y * scale
		});
	button.sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
	button.sprite->Update();
}

ResultMenuCommand ResultMenu::Update(Input* input)
{
	if (!isOpen_ || !input)
	{
		return ResultMenuCommand::None;
	}

	const Vector2 mousePos = input->GetMousePosition();
	const bool leftClick = input->TriggerMouse(0);

	const bool hoverNext =
		(mode_ == ResultMenuMode::GameClear) &&
		IsMouseInside(mousePos, nextStageButton_.basePosition, nextStageButton_.baseSize);

	const bool hoverRetry =
		IsMouseInside(mousePos, retryButton_.basePosition, retryButton_.baseSize);

	const bool hoverTitle =
		IsMouseInside(mousePos, titleButton_.basePosition, titleButton_.baseSize);

	if (mode_ == ResultMenuMode::GameClear)
	{
		UpdateButtonVisual(nextStageButton_, hoverNext);
	}
	UpdateButtonVisual(retryButton_, hoverRetry);
	UpdateButtonVisual(titleButton_, hoverTitle);

	if (leftClick)
	{
		if (mode_ == ResultMenuMode::GameClear && hoverNext)
		{
			return ResultMenuCommand::NextStage;
		}
		if (hoverRetry)
		{
			return ResultMenuCommand::Retry;
		}
		if (hoverTitle)
		{
			return ResultMenuCommand::ToTitle;
		}
	}

	return ResultMenuCommand::None;
}

void ResultMenu::Draw()
{
	if (!isOpen_) { return; }

	if (backdrop_) { backdrop_->Draw(); }

	if (mode_ == ResultMenuMode::GameClear)
	{
		if (clearHeader_) { clearHeader_->Draw(); }
		if (nextStageButton_.sprite) { nextStageButton_.sprite->Draw(); }
	}
	else
	{
		if (gameOverHeader_) { gameOverHeader_->Draw(); }
	}

	if (retryButton_.sprite) { retryButton_.sprite->Draw(); }
	if (titleButton_.sprite) { titleButton_.sprite->Draw(); }
}