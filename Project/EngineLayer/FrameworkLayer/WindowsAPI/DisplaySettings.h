#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

	/// ---------- ウィンドウモード列挙型 ---------- ///
	enum class WindowMode
	{
		Windowed,			  // ウィンドウモード
		BorderlessFullscreen, // ボーダーレスフルスクリーンモード
		ExculusiveFullscreen  // 排他フルスクリーンモード
	};

	/// ---------- 画面設定構造体 ---------- ///
	struct DisplaySettings
	{
		WindowMode mode = WindowMode::Windowed; // ウィンドウモードのデフォルト値

		uint32_t width = 1280;				   // 画面幅のデフォルト値
		uint32_t height = 720;				   // 画面高さのデフォルト値

		int monitorIndex = 0;				   // 使用するモニターのインデックス（0がメインモニター）
		bool maximize = false;				   // ウィンドウモード時に最大化するかどうか
	};
} // namespace Ken4lowEngine
