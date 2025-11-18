#include "ConfirmQuitOverlay.h"
#include <Input.h>
#include <SpriteManager.h>

/// -------------------------------------------------------------
///					　オーバーレイを開く処理
/// -------------------------------------------------------------
void ConfirmQuitOverlay::Open(SceneManager* sceneManager)
{
	BaseOverlay::Open(sceneManager);

	input_ = Input::GetInstance();
	input_->SetLockCursor(false);

	dim_ = std::make_unique<Sprite>(); dim_->Initialize(kWhiteTex);
	dim_->SetPosition({ 0,0 }); dim_->SetSize({ 1280,720 }); dim_->SetAnchorPoint({ 0,0 });
	dim_->SetColor({ 0,0,0,0.5f });

	// パネル
	panel_ = std::make_unique<Sprite>(); panel_->Initialize(kPanelTex);
	panel_->SetAnchorPoint({ 0.5f,0.5f }); panel_->SetPosition({ 640,360 }); panel_->SetSize({ 960,640 });

	// ボタン
	btnYes_ = std::make_unique<Sprite>(); btnYes_->Initialize(kBtnTexYes_);
	btnYes_->SetAnchorPoint({ 0.5f,0.5f }); btnYes_->SetPosition({ rYes_.x + rYes_.width * 0.5f, rYes_.y + rYes_.height * 0.5f }); btnYes_->SetSize({ rYes_.width,rYes_.height });

	// ボタン
	btnNo_ = std::make_unique<Sprite>(); btnNo_->Initialize(kBtnTexNo_);
	btnNo_->SetAnchorPoint({ 0.5f,0.5f });  btnNo_->SetPosition({ rNo_.x + rNo_.width * 0.5f, rNo_.y + rNo_.height * 0.5f });    btnNo_->SetSize({ rNo_.width,rNo_.height });

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
	bool pressedEnter = input_->TriggerKey(DIK_RETURN);
	bool clickedMouse = input_->TriggerMouse(0);

	// Enter キー → フォーカスしているボタンで決定
	if (pressedEnter)
	{
		if (focus_ == 0) { if (onYes_) onYes_(); Close(); }
		else { if (onNo_)  onNo_();  Close(); }
	}
	// マウスクリック → クリック位置がボタンの中のときだけ決定
	else if (clickedMouse)
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
