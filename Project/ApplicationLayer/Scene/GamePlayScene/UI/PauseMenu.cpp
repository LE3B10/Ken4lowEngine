#define NOMINMAX
#include "PauseMenu.h"

#include <DirectXCommon.h>
#include <Input.h>
#include "GameViewportConstants.h"

#include <algorithm>

namespace K4E = ::Ken4lowEngine;

namespace
{
	constexpr const char* kPauseTitleTex = "Resources/Textures/Compiled/UI/Pause/pause_paused_text.dds";
	constexpr const char* kPauseHelpTex = "Resources/Textures/Compiled/UI/Pause/pause_help_text.dds";
	constexpr const char* kButtonTextTex[3] =
	{
		"Resources/Textures/Compiled/UI/Pause/pause_resume_text.dds",
		"Resources/Textures/Compiled/UI/Pause/pause_stage_select_text.dds",
		"Resources/Textures/Compiled/UI/Pause/pause_title_text.dds"
	};
}

void PauseMenu::Initialize()
{
	// Pause menuはWinAppサイズに追従せず固定内部解像度1920x1080で構築する。
	screenWidth_ = static_cast<float>(K4E::GameViewportConstants::Width);
	screenHeight_ = static_cast<float>(K4E::GameViewportConstants::Height);

	overlay_ = CreateWhiteSprite();
	panel_ = CreateWhiteSprite();
	panelBorder_ = CreateWhiteSprite();

	title_ = CreateTextSprite(kPauseTitleTex);
	help_ = CreateTextSprite(kPauseHelpTex);

	buttons_.clear();
	buttons_.reserve(items_.size());

	for (int i = 0; i < static_cast<int>(items_.size()); ++i)
	{
		ButtonSprites b{};
		b.bg = CreateWhiteSprite();
		b.border = CreateWhiteSprite();
		b.accent = CreateWhiteSprite();

		const char* path = (i >= 0 && i < 3) ? kButtonTextTex[i] : "Effects/white.dds";
		b.text = CreateTextSprite(path);

		buttons_.push_back(std::move(b));
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
	if (panel_) panel_->Draw();
	if (panelBorder_) panelBorder_->Draw();

	if (title_) title_->Draw();

	for (auto& b : buttons_)
	{
		if (b.bg) b.bg->Draw();
		if (b.border) b.border->Draw();
		if (b.accent) b.accent->Draw();
		if (b.text) b.text->Draw();
	}

	if (help_) help_->Draw();
}

void PauseMenu::RebuildLayout()
{
	const float cx = screenWidth_ * 0.5f;
	const float cy = screenHeight_ * 0.5f;

	const float panelW = std::min(screenWidth_ * 0.62f, 720.0f);
	const float panelH = std::min(screenHeight_ * 0.58f, 460.0f);
	const float panelX = cx - panelW * 0.5f;
	const float panelY = cy - panelH * 0.5f;

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
		panel_->SetPosition({ panelX, panelY });
		panel_->SetSize({ panelW, panelH });
		panel_->Update();
	}

	// パネル枠
	if (panelBorder_)
	{
		panelBorder_->SetAnchorPoint({ 0.0f, 0.0f });
		panelBorder_->SetPosition({ panelX - 2.0f, panelY - 2.0f });
		panelBorder_->SetSize({ panelW + 4.0f, panelH + 4.0f });
		panelBorder_->Update();
	}

	// タイトル
	if (title_)
	{
		title_->SetAnchorPoint({ 0.5f, 0.0f });
		title_->SetPosition({ cx, panelY + 18.0f });

		auto t = title_->GetTextureSize();
		title_->SetSize({ t.x, t.y });
		title_->Update();
	}

	// ヘルプ
	if (help_)
	{
		help_->SetAnchorPoint({ 0.5f, 1.0f });
		help_->SetPosition({ cx, panelY + panelH - 16.0f });

		auto t = help_->GetTextureSize();
		help_->SetSize({ t.x, t.y });
		help_->Update();
	}

	// ボタン
	const float buttonW = panelW - 72.0f;
	const float buttonH = 58.0f;
	const float gap = 14.0f;
	const float startY = panelY + 92.0f;

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		auto& b = buttons_[i];

		const float x = cx - buttonW * 0.5f;
		const float y = startY + i * (buttonH + gap);

		b.rect.left = x;
		b.rect.top = y;
		b.rect.right = x + buttonW;
		b.rect.bottom = y + buttonH;

		if (b.bg)
		{
			b.bg->SetAnchorPoint({ 0.0f, 0.0f });
			b.bg->SetPosition({ x, y });
			b.bg->SetSize({ buttonW, buttonH });
			b.bg->Update();
		}

		if (b.border)
		{
			b.border->SetAnchorPoint({ 0.0f, 0.0f });
			b.border->SetPosition({ x - 1.0f, y - 1.0f });
			b.border->SetSize({ buttonW + 2.0f, buttonH + 2.0f });
			b.border->Update();
		}

		if (b.accent)
		{
			b.accent->SetAnchorPoint({ 0.0f, 0.5f });
			b.accent->SetPosition({ x + 12.0f, y + buttonH * 0.5f });
			b.accent->SetSize({ 6.0f, buttonH - 16.0f });
			b.accent->Update();
		}

		if (b.text)
		{
			b.text->SetAnchorPoint({ 0.0f, 0.5f });
			b.text->SetPosition({ x + 28.0f, y + buttonH * 0.5f });

			const K4E::Vector2 tex = b.text->GetTextureSize();
			const float scale = 0.82f; // 少し小さめに
			b.text->SetSize({ tex.x * scale, tex.y * scale });
			b.text->Update();
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
	if (panel_) panel_->SetColor({ 0.06f, 0.07f, 0.09f, 0.93f });
	if (panelBorder_) panelBorder_->SetColor({ 0.95f, 0.28f, 0.22f, 0.92f });

	if (title_) title_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	if (help_) help_->SetColor({ 0.85f, 0.88f, 0.94f, 0.85f });

	for (int i = 0; i < static_cast<int>(buttons_.size()); ++i)
	{
		const bool selected = (i == selectedIndex_);
		auto& b = buttons_[i];

		if (b.bg)
		{
			if (selected) { b.bg->SetColor({ 0.20f, 0.05f, 0.05f, 0.94f }); }
			else { b.bg->SetColor({ 0.10f, 0.11f, 0.14f, 0.88f }); }
		}

		if (b.border)
		{
			if (selected) { b.border->SetColor({ 1.0f, 0.55f, 0.25f, 0.95f }); }
			else { b.border->SetColor({ 1.0f, 1.0f, 1.0f, 0.14f }); }
		}

		if (b.accent)
		{
			if (selected) { b.accent->SetColor({ 1.0f, 0.38f, 0.20f, 0.95f }); }
			else { b.accent->SetColor({ 1.0f, 0.38f, 0.20f, 0.00f }); }
		}

		if (b.text)
		{
			if (selected) { b.text->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); }
			else { b.text->SetColor({ 0.82f, 0.86f, 0.93f, 0.95f }); }
		}
	}
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

std::unique_ptr<K4E::Sprite> PauseMenu::CreateTextSprite(const std::string& path)
{
	auto sp = std::make_unique<K4E::Sprite>();
	sp->Initialize(path);
	sp->SetAnchorPoint({ 0.0f, 0.0f });
	sp->SetPosition({ 0.0f, 0.0f });
	const K4E::Vector2 tex = sp->GetTextureSize();
	sp->SetSize({ tex.x, tex.y });
	sp->Update();
	return sp;
}
