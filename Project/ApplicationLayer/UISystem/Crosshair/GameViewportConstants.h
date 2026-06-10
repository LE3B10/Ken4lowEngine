#pragma once
#include <ResolutionManager.h>

namespace Ken4lowEngine
{
	/// <summary>
	/// Crosshair専用の画面サイズ参照です。
	/// </summary>
	struct GameViewportConstants
	{
		struct DynamicWidth
		{
			// Crosshairの中心座標を現在解像度から取得し、1280/1920/2560での表示ズレを防ぐ。
			operator float() const { return ResolutionManager::GetInstance()->GetScreenWidth(); }
			operator uint32_t() const { return static_cast<uint32_t>(ResolutionManager::GetInstance()->GetScreenWidth()); }
		};

		struct DynamicHeight
		{
			// Crosshairの中心座標を現在解像度から取得し、1280/1920/2560での表示ズレを防ぐ。
			operator float() const { return ResolutionManager::GetInstance()->GetScreenHeight(); }
			operator uint32_t() const { return static_cast<uint32_t>(ResolutionManager::GetInstance()->GetScreenHeight()); }
		};

		inline static const DynamicWidth Width{};
		inline static const DynamicHeight Height{};
	};
} // namespace Ken4lowEngine
