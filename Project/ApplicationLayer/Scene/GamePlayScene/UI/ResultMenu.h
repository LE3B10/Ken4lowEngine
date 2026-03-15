#pragma once
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine { class Input; }

enum class ResultMenuMode
{
	GameClear,
	GameOver,
};

enum class ResultMenuCommand
{
	None,
	NextStage,
	Retry,
	ToTitle,
};

/// -------------------------------------------------------------
///				　	リザルトメニュークラス
/// -------------------------------------------------------------
class ResultMenu
{
private: /// ---------- 構造体 ---------- ///

	struct Button
	{
		std::unique_ptr<Ken4lowEngine::Sprite> sprite;
		Ken4lowEngine::Vector2 basePosition{ 0.0f, 0.0f };
		Ken4lowEngine::Vector2 baseSize{ 0.0f, 0.0f };
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 開く処理（modeに応じた見た目の切り替えも行う）
	void Open(ResultMenuMode mode);

	// 終了処理
	void Close();

	// 更新処理（入力に応じたコマンドを返す）
	ResultMenuCommand Update(Ken4lowEngine::Input* input);

	// 描画処理
	void Draw();

	// メニューが開いているかどうか
	bool IsOpen() const { return isOpen_; }

private: /// ---------- メンバ関数 ---------- ///

	// マウスが指定した矩形内にあるかどうか
	bool IsMouseInside(const Ken4lowEngine::Vector2& mousePos, const Ken4lowEngine::Vector2& center, const Ken4lowEngine::Vector2& size) const;

	// ボタンの見た目を更新（hovered: マウスオーバーしているかどうか）
	void UpdateButtonVisual(Button& button, bool hovered);

private: /// ---------- メンバ変数 ---------- ///

	// メニューが開いているかどうか
	bool isOpen_ = false;

	// 現在のモード
	ResultMenuMode mode_ = ResultMenuMode::GameClear;

	// 常に表示するスプライト
	std::unique_ptr<Ken4lowEngine::Sprite> backdrop_;
	std::unique_ptr<Ken4lowEngine::Sprite> clearHeader_;
	std::unique_ptr<Ken4lowEngine::Sprite> gameOverHeader_;

	// 背景のスプライトはモードによって切り替える
	std::unique_ptr<Ken4lowEngine::Sprite> gameClearBackground_;
	std::unique_ptr<Ken4lowEngine::Sprite> gameOverBackground_;

	// ボタンはモードによって表示/非表示を切り替える
	Button nextStageButton_;
	Button retryButton_;
	Button titleButton_;
};

