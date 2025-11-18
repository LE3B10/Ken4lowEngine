#pragma once
#include "BaseOverlay.h"
#include <Sprite.h>
#include <Rect.h>

#include <functional>
#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
///					終了確認オーバーレイクラス
/// -------------------------------------------------------------
class ConfirmQuitOverlay final : public BaseOverlay
{
public: /// ---------- メンバ関数 ---------- ///

	// コールバック関数の型定義
	using Callback = std::function<void()>;

	// オーバーレイを開く処理
	void Open(SceneManager* sceneManager) override;

	// 更新処理
	void Update() override;

	// 2D描画処理
	void Draw2D() override;

	// ワールドを一時停止するかどうか
	bool PausesWorld() const override { return true; }

	// コールバック関数の設定
	void SetCallbacks(Callback onYes, Callback onNo) { onYes_ = std::move(onYes); onNo_ = std::move(onNo); }

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力管理オブジェクトへのポインタ

	std::unique_ptr<Sprite> dim_;
	std::unique_ptr<Sprite> panel_;
	std::unique_ptr<Sprite> btnYes_;
	std::unique_ptr<Sprite> btnNo_;
	Rect rYes_{ 350, 400, 261, 89 };
	Rect rNo_{ 660, 400, 260, 89 };
	int focus_ = 0; // 0:Yes,1:No

	Callback onYes_;
	Callback onNo_;

	const std::string kWhiteTex = "white.png";   // 1x1 の白
	const std::string kPanelTex = "assets/ConfirmOverlay_panel_only.png";   // 任意。無ければ白＋色
	const std::string kBtnTexYes_ = "assets/ConfirmOverlay_button_yes.png";  // 任意
	const std::string kBtnTexNo_ = "assets/ConfirmOverlay_button_no.png";  // 任意
};

