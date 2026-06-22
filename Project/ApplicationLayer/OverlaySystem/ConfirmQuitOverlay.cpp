#include "ConfirmQuitOverlay.h"
#include <Input.h>
#include <SpriteManager.h>
#include <FontAtlasLoader.h>
#include "WinApp.h"
#include "GameViewportConstants.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

namespace
{
	std::unique_ptr<Sprite> CreateSolidSprite(const std::string& texturePath)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(texturePath);
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		return sprite;
	}

	void SetupRectSprite(Sprite* sprite, const Vector2& center, const Vector2& size)
	{
		if (!sprite)
		{
			return;
		}

		sprite->SetPosition(center);
		sprite->SetSize(size);
		sprite->Update();
	}
}

ConfirmQuitOverlay::~ConfirmQuitOverlay()
{
	if (textDrawer_)
	{
		textDrawer_->Finalize();
	}
}

/// -------------------------------------------------------------
///					　オーバーレイを開く処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Open(SceneManager* sceneManager)
{
	BaseOverlay::Open(sceneManager);
	// Confirm UIはWinApp実ウィンドウではなく固定内部解像度1920x1080で配置する。
	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);
	const Vector2 center = { screenW * 0.5f, screenH * 0.5f };

	input_ = Input::GetInstance();
	if (input_)
	{
		input_->SetLockCursor(false);
	}

	// AI生成画像に依存しないよう、白テクスチャを色付き矩形として使う。
	dim_ = CreateSolidSprite(kWhiteTex);
	dim_->SetAnchorPoint({ 0.0f, 0.0f });
	dim_->SetPosition({ 0.0f, 0.0f });
	dim_->SetSize({ screenW, screenH });
	dim_->SetColor({ 0.0f, 0.0f, 0.0f, 0.58f });
	dim_->Update();

	panelBorder_ = CreateSolidSprite(kWhiteTex);
	panel_ = CreateSolidSprite(kWhiteTex);
	titleLine_ = CreateSolidSprite(kWhiteTex);
	btnYesBorder_ = CreateSolidSprite(kWhiteTex);
	btnYes_ = CreateSolidSprite(kWhiteTex);
	btnNoBorder_ = CreateSolidSprite(kWhiteTex);
	btnNo_ = CreateSolidSprite(kWhiteTex);

	const Vector2 panelSize = { 860.0f, 430.0f };
	SetupRectSprite(panelBorder_.get(), center, { panelSize.x + 6.0f, panelSize.y + 6.0f });
	SetupRectSprite(panel_.get(), center, panelSize);
	SetupRectSprite(titleLine_.get(), { center.x, center.y - 84.0f }, { 660.0f, 4.0f });

	rYes_.width = 300.0f;
	rYes_.height = 84.0f;
	rNo_.width = 300.0f;
	rNo_.height = 84.0f;
	rYes_.x = center.x - rYes_.width - 24.0f;
	rYes_.y = center.y + 30.0f;
	rNo_.x = center.x + 24.0f;
	rNo_.y = center.y + 30.0f;

	SetupRectSprite(btnYesBorder_.get(), { rYes_.x + rYes_.width * 0.5f, rYes_.y + rYes_.height * 0.5f }, { rYes_.width + 6.0f, rYes_.height + 6.0f });
	SetupRectSprite(btnYes_.get(), { rYes_.x + rYes_.width * 0.5f, rYes_.y + rYes_.height * 0.5f }, { rYes_.width, rYes_.height });
	SetupRectSprite(btnNoBorder_.get(), { rNo_.x + rNo_.width * 0.5f, rNo_.y + rNo_.height * 0.5f }, { rNo_.width + 6.0f, rNo_.height + 6.0f });
	SetupRectSprite(btnNo_.get(), { rNo_.x + rNo_.width * 0.5f, rNo_.y + rNo_.height * 0.5f }, { rNo_.width, rNo_.height });

	blockTiles_.clear();
	const int columns = 12;
	const int rows = 6;
	const Vector2 tileSize = { panelSize.x / static_cast<float>(columns), panelSize.y / static_cast<float>(rows) };
	const Vector2 panelTopLeft = { center.x - panelSize.x * 0.5f, center.y - panelSize.y * 0.5f };
	blockTiles_.reserve(columns * rows);
	for (int y = 0; y < rows; ++y)
	{
		for (int x = 0; x < columns; ++x)
		{
			auto tile = CreateSolidSprite(kWhiteTex);
			const Vector2 pos = {
				panelTopLeft.x + tileSize.x * (static_cast<float>(x) + 0.5f),
				panelTopLeft.y + tileSize.y * (static_cast<float>(y) + 0.5f)
			};
			SetupRectSprite(tile.get(), pos, { tileSize.x - 2.0f, tileSize.y - 2.0f });
			blockTiles_.push_back(std::move(tile));
		}
	}

	textDrawer_ = std::make_unique<TextSpriteDrawer>();
	textReady_ = false;
	try
	{
		auto fontDefJP = FontAtlasLoader::LoadFromJson(
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

	focus_ = 0;
}

/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Update()
{
	if (!input_)
	{
		return;
	}

	// ← → でフォーカス移動
	if (input_->TriggerKey(DIK_LEFT)) { focus_ = (focus_ + 2 - 1) % 2; }
	if (input_->TriggerKey(DIK_RIGHT)) { focus_ = (focus_ + 1) % 2; }

	// ESC でキャンセル
	if (input_->TriggerKey(DIK_ESCAPE)) { Close(); return; }

	// マウス位置
	Vector2 mp = input_->GetMousePosition();

	// 今のマウス位置がボタン上にあるかどうか
	bool isOverYes = HitRect(mp, rYes_);
	bool isOverNo = HitRect(mp, rNo_);

	// マウスが上に乗っていたらフォーカスを合わせる
	if (isOverYes) { focus_ = 0; }
	else if (isOverNo) { focus_ = 1; }

	// 入力（キーボード / マウス）
	const bool decideByKey = input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_NUMPADENTER) || input_->TriggerKey(DIK_SPACE);
	const bool clickedMouse = input_->TriggerMouse(0);

	if (decideByKey)
	{
		if (focus_ == 0)
		{
			if (onYes_) onYes_();
		}
		else
		{
			if (onNo_) onNo_();
		}
		Close();
		return;
	}

	// マウスクリック → クリック位置がボタンの中のときだけ決定
	if (clickedMouse)
	{
		if (isOverYes)
		{
			if (onYes_) onYes_();
			Close();
		}
		else if (isOverNo)
		{
			if (onNo_) onNo_();
			Close();
		}
	}

	if (dim_) dim_->Update();
	if (panel_) panel_->Update();
	if (panelBorder_) panelBorder_->Update();
	if (titleLine_) titleLine_->Update();

	panelBorder_->SetColor({ 0.95f, 0.48f, 0.14f, 0.95f });
	panel_->SetColor({ 0.045f, 0.040f, 0.035f, 0.94f });
	titleLine_->SetColor({ 0.95f, 0.56f, 0.18f, 0.92f });

	for (int i = 0; i < static_cast<int>(blockTiles_.size()); ++i)
	{
		const float shade = (i % 2 == 0) ? 0.075f : 0.105f;
		blockTiles_[i]->SetColor({ shade, shade * 0.92f, shade * 0.78f, 0.72f });
		blockTiles_[i]->Update();
	}

	auto setButton = [](Sprite* border, Sprite* body, bool hot)
	{
		if (border)
		{
			border->SetColor(hot ? Vector4{ 1.0f, 0.64f, 0.18f, 1.0f } : Vector4{ 0.35f, 0.30f, 0.22f, 1.0f });
			border->Update();
		}
		if (body)
		{
			body->SetColor(hot ? Vector4{ 0.32f, 0.18f, 0.06f, 0.98f } : Vector4{ 0.12f, 0.11f, 0.10f, 0.95f });
			body->Update();
		}
	};

	setButton(btnYesBorder_.get(), btnYes_.get(), focus_ == 0);
	setButton(btnNoBorder_.get(), btnNo_.get(), focus_ == 1);
}

/// -------------------------------------------------------------
///					　		2D描画処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Draw2D()
{
	if (dim_) dim_->Draw();
	if (panelBorder_) panelBorder_->Draw();
	if (panel_) panel_->Draw();
	for (auto& tile : blockTiles_)
	{
		if (tile) tile->Draw();
	}
	if (titleLine_) titleLine_->Draw();
	if (btnYesBorder_) btnYesBorder_->Draw();
	if (btnYes_) btnYes_->Draw();
	if (btnNoBorder_) btnNoBorder_->Draw();
	if (btnNo_) btnNo_->Draw();

	if (!textReady_ || !textDrawer_)
	{
		return;
	}

	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);
	const Vector2 center = { screenW * 0.5f, screenH * 0.5f };

	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(2.0f);
	textDrawer_->SetLineSpacing(6.0f);

	textDrawer_->SetScale(1.25f);
	textDrawer_->SetColor({ 1.0f, 0.90f, 0.52f, 1.0f });
	textDrawer_->DrawTextCentered("ゲームを終了しますか？", { center.x, center.y - 145.0f });

	textDrawer_->SetScale(0.95f);
	textDrawer_->SetColor(focus_ == 0 ? Vector4{ 1.0f, 0.94f, 0.50f, 1.0f } : Vector4{ 0.78f, 0.76f, 0.70f, 1.0f });
	textDrawer_->DrawTextCentered("はい", { rYes_.x + rYes_.width * 0.5f, rYes_.y + rYes_.height * 0.4f });

	textDrawer_->SetColor(focus_ == 1 ? Vector4{ 1.0f, 0.94f, 0.50f, 1.0f } : Vector4{ 0.78f, 0.76f, 0.70f, 1.0f });
	textDrawer_->DrawTextCentered("いいえ", { rNo_.x + rNo_.width * 0.5f, rNo_.y + rNo_.height * 0.4f });

	textDrawer_->SetScale(0.52f);
	textDrawer_->SetLetterSpacing(1.0f);
	textDrawer_->SetColor({ 0.78f, 0.80f, 0.82f, 0.92f });
	textDrawer_->DrawTextCentered("← / → 選択    Enter / Click 決定    ESC キャンセル", { center.x, center.y + 175.0f });
}
