#define NOMINMAX
#include "PauseMenu.h"

#include <DirectXCommon.h>
#include <FontAtlasLoader.h>
#include <Input.h>
#include <TextSpriteDrawer.h>
#include "GameViewportConstants.h"

#include <algorithm>

namespace K4E = ::Ken4lowEngine;

PauseMenu::~PauseMenu()
{
	if (textDrawer_)
	{
		textDrawer_->Finalize();
	}
}

void PauseMenu::Initialize()
{
	// Pause menuはWinAppサイズに追従せず固定内部解像度1920x1080で構築する。
	screenWidth_ = static_cast<float>(K4E::GameViewportConstants::Width);
	screenHeight_ = static_cast<float>(K4E::GameViewportConstants::Height);

	// UI生成画像に依存しないよう、白スプライトの矩形とTextSpriteDrawerでポーズ画面を構築する。
	overlay_ = CreateWhiteSprite();
	panel_ = CreateWhiteSprite();
	panelBorder_ = CreateWhiteSprite();
	titleLine_ = CreateWhiteSprite();

	buttons_.clear();
	buttons_.reserve(items_.size());

	for (int i = 0; i < static_cast<int>(items_.size()); ++i)
	{
		ButtonSprites b{};
		b.bg = CreateWhiteSprite();
		b.border = CreateWhiteSprite();
		b.accent = CreateWhiteSprite();
		buttons_.push_back(std::move(b));
	}

	textDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	textReady_ = false;
	try
	{
		auto fontDefJP = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
		textDrawer_->Initialize(fontDefJP);
		textReady_ = true;
	}
	catch (...)
	{
		textReady_ = false;
	}

	selectedIndex_ = 0;
	isOpen_ = false;

	RebuildLayout();
	ApplyVisualState();
}

void PauseMenu::Open()
{
	isOpen_ = true;
	selectedIndex_ = 0;
	ApplyVisualState();
}

void PauseMenu::Close()
{
	isOpen_ = false;
}

PauseMenuCommand PauseMenu::Update(Ken4lowEngine::Input* input)
{
	if (!isOpen_ || !input)
	{
		return PauseMenuCommand::None;
	}

	RefreshScreenSizeIfNeeded();

	const int itemCount = static_cast<int>(items_.size());
	if (itemCount <= 0)
	{
		return PauseMenuCommand::None;
	}

	auto moveCursor = [&](int delta)
		{
			selectedIndex_ += delta;
			if (selectedIndex_ < 0) { selectedIndex_ = itemCount - 1; }
			if (selectedIndex_ >= itemCount) { selectedIndex_ = 0; }
		};

	// 上下移動（W/S, 矢印キー）
	if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP))
	{
		moveCursor(-1);
	}
	if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN))
	{
		moveCursor(+1);
	}

	// マウスホバーで選択変更
	const K4E::Vector2 mousePos = input->GetMousePosition();
	const int hoverIndex = HitTestButtonIndex(mousePos);
	if (hoverIndex >= 0)
	{
		selectedIndex_ = hoverIndex;
	}

	ApplyVisualState();

	// 決定（Enter / NumpadEnter / Space / 左クリック）
	const bool decideByKey =
		input->TriggerKey(DIK_RETURN) ||
		input->TriggerKey(DIK_NUMPADENTER) ||
		input->TriggerKey(DIK_SPACE);

	const bool decideByMouse = (hoverIndex >= 0) && input->TriggerMouse(0);

	if (!(decideByKey || decideByMouse))
	{
		return PauseMenuCommand::None;
	}

	switch (selectedIndex_)
	{
	case 0: return PauseMenuCommand::Resume;
	case 1: return PauseMenuCommand::ToStageSelect;
	case 2: return PauseMenuCommand::ToTitle;
	default: break;
	}

	return PauseMenuCommand::None;
}

void PauseMenu::Draw()
{
	if (!isOpen_)
	{
		return;
	}

	RefreshScreenSizeIfNeeded();
	ApplyVisualState();

	if (overlay_) overlay_->Draw();
	if (panelBorder_) panelBorder_->Draw();
	if (panel_) panel_->Draw();
	if (titleLine_) titleLine_->Draw();

	for (auto& b : buttons_)
	{
		if (b.border) b.border->Draw();
		if (b.bg) b.bg->Draw();
		if (b.accent) b.accent->Draw();
	}

	DrawTexts();
}

void PauseMenu::RebuildLayout()
{
	const float cx = screenWidth_ * 0.5f;
	const float cy = screenHeight_ * 0.5f;

	panelW_ = std::min(screenWidth_ * 0.62f, 760.0f);
	panelH_ = std::min(screenHeight_ * 0.58f, 460.0f);
	panelX_ = cx - panelW_ * 0.5f;
	panelY_ = cy - panelH_ * 0.5f;

	// 全画面オーバーレイ
	if (overlay_)
	{
		overlay_->SetAnchorPoint({ 0.0f, 0.0f });
		overlay_->SetPosition({ 0.0f, 0.0f });
		overlay_->SetSize({ screenWidth_, screenHeight_ });
		overlay_->Update();
	}

	// パネル本体
	if (panel_)
	{
		panel_->SetAnchorPoint({ 0.0f, 0.0f });
		panel_->SetPosition({ panelX_, panelY_ });
		panel_->SetSize({ panelW_, panelH_ });
		panel_->Update();
	}

	// パネル枠
	if (panelBorder_)
	{
		panelBorder_->SetAnchorPoint({ 0.0f, 0.0f });
		panelBorder_->SetPosition({ panelX_ - 3.0f, panelY_ - 3.0f });
		panelBorder_->SetSize({ panelW_ + 6.0f, panelH_ + 6.0f });
		panelBorder_->Update();
	}

	// タイトル下のライン
	if (titleLine_)
	{
		titleLine_->SetAnchorPoint({ 0.5f, 0.0f });
		titleLine_->SetPosition({ cx, panelY_ + 86.0f });
		titleLine_->SetSize({ panelW_ - 120.0f, 4.0f });
		titleLine_->Update();
	}

	// ボタン
	const float buttonW = panelW_ - 96.0f;
	const float buttonH = 64.0f;
	const float gap = 16.0f;
	const float startY = panelY_ + 118.0f;

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		auto& b = buttons_[i];

		const float x = cx - buttonW * 0.5f;
		const float y = startY + i * (buttonH + gap);

		b.rect.left = x;
		b.rect.top = y;
		b.rect.right = x + buttonW;
		b.rect.bottom = y + buttonH;

		if (b.border)
		{
			b.border->SetAnchorPoint({ 0.0f, 0.0f });
			b.border->SetPosition({ x - 2.0f, y - 2.0f });
			b.border->SetSize({ buttonW + 4.0f, buttonH + 4.0f });
			b.border->Update();
		}

		if (b.bg)
		{
			b.bg->SetAnchorPoint({ 0.0f, 0.0f });
			b.bg->SetPosition({ x, y });
			b.bg->SetSize({ buttonW, buttonH });
			b.bg->Update();
		}

		if (b.accent)
		{
			b.accent->SetAnchorPoint({ 0.0f, 0.5f });
			b.accent->SetPosition({ x + 14.0f, y + buttonH * 0.5f });
			b.accent->SetSize({ 7.0f, buttonH - 18.0f });
			b.accent->Update();
		}
	}
}

void PauseMenu::RefreshScreenSizeIfNeeded()
{
	// 固定内部解像度は正値定数なので、定数条件チェックでC4127を出さない。
	const float w = static_cast<float>(K4E::GameViewportConstants::Width);
	const float h = static_cast<float>(K4E::GameViewportConstants::Height);

	if (w != screenWidth_ || h != screenHeight_)
	{
		screenWidth_ = w;
		screenHeight_ = h;
		RebuildLayout();
	}
}

int PauseMenu::HitTestButtonIndex(const K4E::Vector2& mousePos) const
{
	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		if (mousePos.x >= buttons_[i].rect.left && mousePos.x <= buttons_[i].rect.right && mousePos.y >= buttons_[i].rect.top && mousePos.y <= buttons_[i].rect.bottom)
		{
			return i;
		}
	}
	return -1;
}

void PauseMenu::ApplyVisualState()
{
	// 背景・パネル
	if (overlay_) overlay_->SetColor({ 0.0f, 0.0f, 0.0f, 0.62f });
	if (panel_) panel_->SetColor({ 0.045f, 0.040f, 0.035f, 0.94f });
	if (panelBorder_) panelBorder_->SetColor({ 0.95f, 0.48f, 0.14f, 0.95f });
	if (titleLine_) titleLine_->SetColor({ 0.95f, 0.56f, 0.18f, 0.92f });

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		const bool selected = (i == selectedIndex_);
		auto& b = buttons_[i];

		if (b.bg)
		{
			if (selected) { b.bg->SetColor({ 0.32f, 0.18f, 0.06f, 0.98f }); }
			else { b.bg->SetColor({ 0.12f, 0.11f, 0.10f, 0.95f }); }
		}

		if (b.border)
		{
			if (selected) { b.border->SetColor({ 1.0f, 0.64f, 0.18f, 1.0f }); }
			else { b.border->SetColor({ 0.35f, 0.30f, 0.22f, 1.0f }); }
		}

		if (b.accent)
		{
			if (selected) { b.accent->SetColor({ 1.0f, 0.40f, 0.18f, 0.96f }); }
			else { b.accent->SetColor({ 1.0f, 0.40f, 0.18f, 0.00f }); }
		}
	}
}

void PauseMenu::DrawTexts()
{
	if (!textReady_ || !textDrawer_)
	{
		return;
	}

	const float cx = screenWidth_ * 0.5f;
	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(2.0f);
	textDrawer_->SetLineSpacing(6.0f);

	textDrawer_->SetScale(1.35f);
	textDrawer_->SetColor({ 1.0f, 0.90f, 0.52f, 1.0f });
	textDrawer_->DrawTextCentered("一時停止", { cx, panelY_ + 34.0f });

	textDrawer_->SetScale(0.92f);
	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		const auto& b = buttons_[i];
		const bool selected = (i == selectedIndex_);
		const float centerX = (b.rect.left + b.rect.right) * 0.5f;
		const float centerY = (b.rect.top + b.rect.bottom) * 0.5f;

		textDrawer_->SetColor(selected ? K4E::Vector4{ 1.0f, 0.94f, 0.50f, 1.0f } : K4E::Vector4{ 0.78f, 0.76f, 0.70f, 1.0f });
		textDrawer_->DrawTextCentered(items_[i], { centerX, centerY - 8.0f });
	}

	textDrawer_->SetScale(0.50f);
	textDrawer_->SetLetterSpacing(1.0f);
	textDrawer_->SetColor({ 0.78f, 0.80f, 0.82f, 0.92f });
	textDrawer_->DrawTextCentered("W / S / ↑ / ↓ 選択    Enter / Click 決定", { cx, panelY_ + panelH_ - 34.0f });
}

std::unique_ptr<K4E::Sprite> PauseMenu::CreateWhiteSprite()
{
	auto sp = std::make_unique<K4E::Sprite>();
	sp->Initialize("Effects/white.dds");
	sp->SetPosition({ 0.0f, 0.0f });
	sp->SetSize({ 1.0f, 1.0f });
	sp->SetAnchorPoint({ 0.0f, 0.0f });
	sp->Update();
	return sp;
}
