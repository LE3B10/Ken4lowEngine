#pragma once
#include "BaseOverlay.h"
#include <Sprite.h>
#include <Rect.h>

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

/// -------------------------------------------------------------
///                      ポーズオーバーレイクラス
/// -------------------------------------------------------------
class PauseOverlay final : public BaseOverlay
{
public:
    PauseOverlay() = default;
    ~PauseOverlay() override = default;

    /// <summary>
    /// オーバーレイを開く
    /// </summary>
    void Open(SceneManager* sceneManager) override;

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update() override;

    /// <summary>
    /// 2D描画
    /// </summary>
    void Draw2D() override;

    /// <summary>
    /// タイトルへ遷移する要求かどうか
    /// </summary>
    bool IsGoTitle() const override { return goTitle_; }

private:
    /// ボタンの当たり判定用
    struct ButtonRect {
        float x;
        float y;
        float w;
        float h;
    };

    /// スプライト生成＆レイアウト
    void InitializeSprites();

    /// キーボードでフォーカス移動（↑↓ / W・S）
    void UpdateFocusByKey();

    /// マウスでフォーカス移動（ホバー）
    void UpdateFocusByMouse();

    /// 現在フォーカス中の項目を決定
    void DecideCurrent();

    /// ボタンのフォーカス強調
    void setEmphasis(K4E::Sprite* sprite, bool emphasized);

private:
    // 入力
    K4E::Input* input_ = nullptr;

    // 背景暗転 & パネル
    std::unique_ptr<K4E::Sprite> dim_;
    std::unique_ptr<K4E::Sprite> panel_;

    // ボタン（ゲームに戻る・タイトルへ）
    std::unique_ptr<K4E::Sprite> btnContinue_;
    std::unique_ptr<K4E::Sprite> btnTitle_;

    // クリック判定用
    ButtonRect rectContinue_{};
    ButtonRect rectTitle_{};

    // どのボタンが選択中か
    // 0:Continue, 1:Title
    int focus_ = 0;

    bool goTitle_ = false;

    // 使用するテクスチャ名
    const std::string kWhiteTex = "white.png";                           // 1x1 の白
    const std::string kPanelTex = "assets/PauseOverlay_panel_only.png";  // パネル用

    // ボタン用（画像テクスチャ）
    const std::string kBtnContinueTex = "assets/ConfirmOverlay_button_return_dot.png";
    const std::string kBtnTitleTex = "assets/ConfirmOverlay_button_title_dot.png";
};
