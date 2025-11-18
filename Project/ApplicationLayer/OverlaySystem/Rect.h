#pragma once
#include <Vector2.h>

/// -------------------------------------------------------------
///					　	矩形構造体
/// -------------------------------------------------------------
struct Rect
{
	float x;      // X座標
	float y;      // Y座標
	float width;  // 幅
	float height; // 高さ
};

/// -------------------------------------------------------------
///				　　　点と矩形の当たり判定
/// -------------------------------------------------------------
static bool HitRect(const Vector2& point, const Rect& rect)
{
	return (point.x >= rect.x) && (point.x <= rect.x + rect.width) &&
		(point.y >= rect.y) && (point.y <= rect.y + rect.height);
}