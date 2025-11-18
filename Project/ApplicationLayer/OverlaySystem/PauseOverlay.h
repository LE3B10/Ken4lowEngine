#pragma once
#include "BaseOverlay.h"
#include <Sprite.h>
#include <Rect.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;

/// -------------------------------------------------------------
///					　	ポーズオーバーレイクラス
/// -------------------------------------------------------------
class PauseOverlay final : public BaseOverlay
{
public: /// ---------- メンバ関数 ---------- ///
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
	/// オブジェクトのタイトルが "Go" であるかどうかを示す真偽値を返します。
	/// </summary>
	/// <returns>タイトルが "Go" の場合は true、そうでない場合は false を返します。</returns>
	bool IsGoTitle() const override { return goTitle_; }

private: /// ---------- 内部構造 ---------- ///

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
	void setEmphasis(Sprite* sprite, bool emphasized);

private: /// ---------- メンバ変数 ---------- ///

	// 入力
	Input* input_ = nullptr;

	// 背景暗転 & パネル
	std::unique_ptr<Sprite> dim_;
	std::unique_ptr<Sprite> panel_;

	// ボタン（再開・設定・タイトルへ）
	std::unique_ptr<Sprite> btnContinue_;
	std::unique_ptr<Sprite> btnSettings_;
	std::unique_ptr<Sprite> btnTitle_;

	// クリック判定用
	ButtonRect rectContinue_{};
	ButtonRect rectSettings_{};
	ButtonRect rectTitle_{};

	// レイアウト（1280x720 前提のときの参考値。実際の当たりは ButtonRect で管理）
	Rect rContinue_{ 490, 280, 300, 60 };
	Rect rSettings_{ 490, 360, 300, 60 };
	Rect rTitle_{ 490, 440, 300, 60 };

	// どのボタンが選択中か
	// 0:Continue, 1:Settings, 2:Title
	int focus_ = 0;

	bool goTitle_ = false;

	// 使用するテクスチャ名
	// ※パネルはさっき作ったパネル画像に差し替えてOK（ファイルパスは自分の環境に合わせてね）
	const std::string kWhiteTex = "white.png";                       // 1x1 の白
	const std::string kPanelTex = "assets/PauseOverlay_panel_only.png";   // パネル用
	const std::string kBtnTex = "white.png";                       // ボタン用（白＋色で矩形ボタン）
};