#pragma once
#include <Sprite.h>

#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

// ポーズメニューのコマンド
enum class PauseMenuCommand
{
	None,
	Resume,         // ゲームに戻る
	ToStageSelect,  // ステージセレクトへ
	ToTitle         // タイトルへ
};

/// -------------------------------------------------------------
///					ポーズ画面を管理するクラス（Spriteベース）
/// -------------------------------------------------------------
class PauseMenu
{
public: /// ---------- メンバ関数 ---------- ///

	void Initialize();

	void Open();

	void Close();

	PauseMenuCommand Update(Ken4lowEngine::Input* input);

	void Draw(); // Spriteベース描画（ImGui不使用）

	bool IsOpen() const { return isOpen_; }

private: /// ---------- 内部構造体 ---------- ///

	struct Rect
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	struct ButtonSprites
	{
		std::unique_ptr<K4E::Sprite> bg;
		std::unique_ptr<K4E::Sprite> border;
		std::unique_ptr<K4E::Sprite> accent;
		std::unique_ptr<K4E::Sprite> text;
		Rect rect{};
	};

private: /// ---------- メンバ関数 ---------- ///

	void RebuildLayout();
	void RefreshScreenSizeIfNeeded();
	int HitTestButtonIndex(const K4E::Vector2& mousePos) const;
	void ApplyVisualState();

	static std::unique_ptr<K4E::Sprite> CreateWhiteSprite();
	static std::unique_ptr<K4E::Sprite> CreateTextSprite(const std::string& path);

private: /// ---------- メンバ変数 ---------- ///

	bool isOpen_ = false;
	int selectedIndex_ = 0;
	float screenWidth_ = 1280.0f;
	float screenHeight_ = 720.0f;

	std::vector<std::string> items_ =
	{
		"RESUME",
		"STAGE SELECT",
		"TITLE"
	};

	// 背景 / パネル
	std::unique_ptr<K4E::Sprite> overlay_;
	std::unique_ptr<K4E::Sprite> panel_;
	std::unique_ptr<K4E::Sprite> panelBorder_;
	std::unique_ptr<K4E::Sprite> title_;
	std::unique_ptr<K4E::Sprite> help_;

	// ボタン群（items_ と同じ順番）
	std::vector<ButtonSprites> buttons_;
};
