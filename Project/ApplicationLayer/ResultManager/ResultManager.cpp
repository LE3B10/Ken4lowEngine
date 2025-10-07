#include "ResultManager.h"
#include <Input.h>

void ResultManager::Initialize()
{
	resultBackground_ = std::make_unique<Sprite>();
	resultBackground_->Initialize("white.png");
	resultBackground_->SetPosition({ 0.0f, 0.0f });
	resultBackground_->SetSize({ 1280.0f, 720.0f });
	resultBackground_->SetColor({ 0.0f, 0.0f, 0.0f, 0.7f }); // 半透明の黒背景

	restartText_ = std::make_unique<Sprite>();
	restartText_->Initialize("white.png");
	restartText_->SetPosition({ 500.0f, 600.0f });
	restartText_->SetSize({ 280.0f, 80.0f });
	restartText_->SetColor({ 1.0f, 0.7f, 0.7f, 1.0f }); // 赤色

	titleText_ = std::make_unique<Sprite>();
	titleText_->Initialize("white.png");
	titleText_->SetPosition({ 500.0f, 500.0f });
	titleText_->SetSize({ 280.0f, 80.0f });
	titleText_->SetColor({ 0.7f, 0.7f, 1.0f, 1.0f }); // 青色

	scoreDrawer_ = std::make_unique<NumberSpriteDrawer>();
	scoreDrawer_->Initialize("number.png");

	killDrawer_ = std::make_unique<NumberSpriteDrawer>();
	killDrawer_->Initialize("number.png");

	/*waveDrawer_ = std::make_unique<NumberSpriteDrawer>();
	waveDrawer_->Initialize("Resources/number.png");*/
}

void ResultManager::Update()
{
	Vector2 mousePos = Input::GetInstance()->GetMousePosition();

	// リスタートボタンの範囲判定
	Vector2 restartPos = restartText_->GetPosition();
	Vector2 restartSize = restartText_->GetSize();
	if (Input::GetInstance()->TriggerMouse(0))
	{
		if (mousePos.x >= restartPos.x && mousePos.x <= restartPos.x + restartSize.x &&
			mousePos.y >= restartPos.y && mousePos.y <= restartPos.y + restartSize.y) {
			restartRequested_ = true;
		}
	}

	// タイトルへ戻るボタンの範囲判定
	Vector2 titlePos = titleText_->GetPosition();
	Vector2 titleSize = titleText_->GetSize();
	if (Input::GetInstance()->TriggerMouse(0))
	{
		if (mousePos.x >= titlePos.x && mousePos.x <= titlePos.x + titleSize.x &&
			mousePos.y >= titlePos.y && mousePos.y <= titlePos.y + titleSize.y) {
			returnToTitleRequested_ = true;
		}
	}

	resultBackground_->Update();
	restartText_->Update();
	titleText_->Update();
}

void ResultManager::Draw()
{
	resultBackground_->Draw();
	restartText_->Draw();
	titleText_->Draw();

	scoreDrawer_->Reset();
	scoreDrawer_->DrawNumberCentered(finalScore_, { 628.0f, 200.0f }, 24.0f);

	killDrawer_->Reset();
	killDrawer_->DrawNumberCentered(killCount_, { 628.0f, 280.0f }, 24.0f);

	/*waveDrawer_->Reset();
	waveDrawer_->DrawNumberCentered(waveCount_, { 628.0f, 360.0f }, 24.0f);*/
}
