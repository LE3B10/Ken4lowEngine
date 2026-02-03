#include "PauseOverlay.h"
#include "SceneManager.h"
#include <Input.h>
#include <DirectXCommon.h>
#include <algorithm>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                      オーバーレイを開く処理
/// -------------------------------------------------------------
void PauseOverlay::Open(SceneManager* sceneManager)
{
    // 基底クラスの Open を呼ぶ（sceneManager_ に保存される想定）
    BaseOverlay::Open(sceneManager);

    // 入力管理オブジェクトの取得
    input_ = K4E::Input::GetInstance();

    // スプライト生成＆レイアウト
    InitializeSprites();

    // 最初は「ゲームに戻る」選択
    focus_ = 0;
    setEmphasis(btnContinue_.get(), true);
    setEmphasis(btnTitle_.get(), false);
}

/// -------------------------------------------------------------
///                      スプライト初期化＆レイアウト
/// -------------------------------------------------------------
void PauseOverlay::InitializeSprites()
{
    K4E::DirectXCommon* dxCommon = K4E::DirectXCommon::GetInstance();

    const float screenW = static_cast<float>(dxCommon->GetClientWidth());
    const float screenH = static_cast<float>(dxCommon->GetClientHeight());

    // --- 背景暗転 ---
    dim_ = std::make_unique<K4E::Sprite>();
    dim_->Initialize(kWhiteTex.c_str());
    dim_->SetAnchorPoint({ 0.0f, 0.0f });
    dim_->SetPosition({ 0.0f, 0.0f });
    dim_->SetSize({ screenW, screenH });
    dim_->SetColor({ 0.0f, 0.0f, 0.0f, 0.5f });

    // --- パネル ---
    panel_ = std::make_unique<K4E::Sprite>();
    panel_->Initialize(kPanelTex.c_str());
    panel_->SetAnchorPoint({ 0.5f, 0.5f });
    panel_->SetPosition({ screenW * 0.5f, screenH * 0.5f });
    panel_->SetSize({ 960.0f, 640.0f });
    panel_->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });

    // --- ボタン共通 ---
    const float btnW = 210.0f;
    const float btnH = 70.0f;

    const float centerX = screenW * 0.5f;

    // 2つ縦並びを画面中央に配置
    const float gapY = 20.0f;
    const float totalH = btnH * 2.0f + gapY;

    // パネル画像の余白を考慮して少し下へ
    const float yOffset = 11.0f * (screenH / 720.0f); // 720基準
    const float groupCenterY = (screenH * 0.5f) + yOffset;
    const float topY = groupCenterY - (totalH * 0.5f) + (btnH * 0.5f);

    auto makeButton = [&](std::unique_ptr<K4E::Sprite>& dst, const std::string& tex, float cx, float cy)
        {
            dst = std::make_unique<K4E::Sprite>();
            dst->Initialize(tex.c_str());
            dst->SetAnchorPoint({ 0.5f, 0.5f });
            dst->SetPosition({ cx, cy });
            dst->SetSize({ btnW, btnH });
            dst->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        };

    // ゲームに戻る / タイトルへ
    makeButton(btnContinue_, kBtnContinueTex, centerX, topY);
    makeButton(btnTitle_, kBtnTitleTex, centerX, topY + (btnH + gapY));

    // クリック判定用の矩形（左上原点）
    rectContinue_ = { centerX - btnW * 0.5f, topY - btnH * 0.5f, btnW, btnH };
    rectTitle_ = { centerX - btnW * 0.5f, (topY + (btnH + gapY)) - btnH * 0.5f, btnW, btnH };
}

/// -------------------------------------------------------------
///                      更新処理
/// -------------------------------------------------------------
void PauseOverlay::Update()
{
    if (dim_) dim_->Update();
    if (panel_) panel_->Update();
    if (btnContinue_) btnContinue_->Update();
    if (btnTitle_) btnTitle_->Update();

    if (!input_) { return; }

    // キーボードで選択移動
    UpdateFocusByKey();

    // マウスホバーでフォーカス移動
    UpdateFocusByMouse();

    // 決定（Enter / Space / 左クリック）
    const bool decideKey = input_->TriggerKey(DIK_RETURN) || input_->TriggerKey(DIK_SPACE);
    const bool decideMouse = input_->TriggerMouse(0);

    // キーボード決定
    if (decideKey)
    {
        DecideCurrent();
    }
    // マウス決定
    else if (decideMouse)
    {
        auto mousePos = input_->GetMousePosition();

        auto inside = [](const ButtonRect& r, const decltype(mousePos)& p)
            {
                return (p.x >= r.x && p.x <= r.x + r.w &&
                    p.y >= r.y && p.y <= r.y + r.h);
            };

        bool clickedAny = false;

        if (inside(rectContinue_, mousePos)) {
            focus_ = 0;
            clickedAny = true;
        }
        else if (inside(rectTitle_, mousePos)) {
            focus_ = 1;
            clickedAny = true;
        }

        if (clickedAny)
        {
            DecideCurrent();
        }
    }

    // フォーカス中のボタンだけ強調
    setEmphasis(btnContinue_.get(), focus_ == 0);
    setEmphasis(btnTitle_.get(), focus_ == 1);
}

/// -------------------------------------------------------------
///                      キーボードでフォーカス移動
/// -------------------------------------------------------------
void PauseOverlay::UpdateFocusByKey()
{
    const int prev = focus_;

    if (input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W))
    {
        focus_ = (focus_ + 2 - 1) % 2; // 上へ
    }
    else if (input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S))
    {
        focus_ = (focus_ + 1) % 2;     // 下へ
    }

    if (focus_ != prev)
    {
        // TODO: カーソル移動 SE を鳴らすならここ
    }
}

/// -------------------------------------------------------------
///                      マウスでフォーカス移動
/// -------------------------------------------------------------
void PauseOverlay::UpdateFocusByMouse()
{
    auto mousePos = input_->GetMousePosition();

    auto inside = [](const ButtonRect& r, const decltype(mousePos)& p)
        {
            return (p.x >= r.x && p.x <= r.x + r.w &&
                p.y >= r.y && p.y <= r.y + r.h);
        };

    if (inside(rectContinue_, mousePos)) {
        focus_ = 0;
    }
    else if (inside(rectTitle_, mousePos)) {
        focus_ = 1;
    }
}

/// -------------------------------------------------------------
///                      現在フォーカス中の項目を決定
/// -------------------------------------------------------------
void PauseOverlay::DecideCurrent()
{
    switch (focus_)
    {
    case 0: // Continue（ゲーム再開）
        // ただ閉じるだけ。GamePlayScene 側で IsClose() を見て Playing に戻る
        close_ = true;
        break;

    case 1: // Title（タイトルに戻る）
        if (input_)
        {
            input_->SetLockCursor(false);
            ShowCursor(true);
        }

        if (sceneManager_)
        {
            sceneManager_->ChangeScene("TitleScene");
        }

        goTitle_ = true;
        close_ = true;
        break;
    }
}

/// -------------------------------------------------------------
///                      ボタンの強調表示
/// -------------------------------------------------------------
void PauseOverlay::setEmphasis(K4E::Sprite* sprite, bool emphasized)
{
    if (!sprite) { return; }

    // 画像ボタンなので、色で“明るさ”だけ変える
    sprite->SetColor(emphasized ? K4E::Vector4{ 1,1,1,1 } : K4E::Vector4{ 0.55f,0.55f,0.55f,1.0f });
}

/// -------------------------------------------------------------
///                          2D描画処理
/// -------------------------------------------------------------
void PauseOverlay::Draw2D()
{
    if (dim_) { dim_->Draw(); }
    if (panel_) { panel_->Draw(); }

    if (btnContinue_) { btnContinue_->Draw(); }
    if (btnTitle_) { btnTitle_->Draw(); }
}
