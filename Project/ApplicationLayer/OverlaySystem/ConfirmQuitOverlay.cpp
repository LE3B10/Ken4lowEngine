#include "ConfirmQuitOverlay.h"
#include <Input.h>
#include <SpriteManager.h>
#include "WinApp.h"
#include "GameViewportConstants.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///					　オーバーレイを開く処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Open(SceneManager* sceneManager)
{
	BaseOverlay::Open(sceneManager);
	// Confirm UIはWinApp実ウィンドウではなく固定内部解像度1920x1080で配置する。
	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);

	input_ = Input::GetInstance();
	input_->SetLockCursor(false);

	dim_ = std::make_unique<Sprite>(); dim_->Initialize(kWhiteTex);
	dim_->SetPosition({ 0,0 }); dim_->SetSize({ screenW, screenH }); dim_->SetAnchorPoint({ 0,0 });
	dim_->SetColor({ 0,0,0,0.5f });

	// パネル
	panel_ = std::make_unique<Sprite>(); panel_->Initialize(kPanelTex);
	panel_->SetAnchorPoint({ 0.5f,0.5f }); panel_->SetPosition({ screenW * 0.5f, screenH * 0.5f }); panel_->SetSize({ 960,640 });

	// ボタン
	btnYes_ = std::make_unique<Sprite>(); btnYes_->Initialize(kBtnTexYes_);
	rYes_.x = (screenW * 0.5f - (rYes_.width + rNo_.width) * 0.5f - 10.0f);
	rYes_.y = (screenH * 0.5f);
	btnYes_->SetAnchorPoint({ 0.5f,0.5f }); btnYes_->SetPosition({ rYes_.x + rYes_.width * 0.5f, rYes_.y + rYes_.height * 0.5f }); btnYes_->SetSize({ rYes_.width,rYes_.height });

	// ボタン
	btnNo_ = std::make_unique<Sprite>(); btnNo_->Initialize(kBtnTexNo_);
	btnNo_->SetAnchorPoint({ 0.5f,0.5f });
	
	rNo_.x = (screenW * 0.5f + (rYes_.width + rNo_.width) * 0.5f + 10.0f - rNo_.width);
	rNo_.y = (screenH * 0.5f);

	btnNo_->SetPosition({ rNo_.x + rNo_.width * 0.5f, rNo_.y + rNo_.height * 0.5f });    btnNo_->SetSize({ rNo_.width,rNo_.height });

}

/// -------------------------------------------------------------
///					　		更新処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Update()
{
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
	bool clickedMouse = input_->TriggerMouse(0);

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

	// ボタンの色設定用ラムダ
	auto setHot = [](Sprite* s, bool hot) {
		// ホットなら白、そうでなければグレー
		s->SetColor(hot ? Vector4{ 1,1,1,1 } : Vector4{ 0.4f,0.4f,0.4f,1.0f });
		s->Update();
		};

	dim_->Update();
	panel_->Update();
	setHot(btnYes_.get(), focus_ == 0);
	setHot(btnNo_.get(), focus_ == 1);
}

/// -------------------------------------------------------------
///					　		2D描画処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Draw2D()
{
	dim_->Draw();
	panel_->Draw();
	btnYes_->Draw();
	btnNo_->Draw();
}
