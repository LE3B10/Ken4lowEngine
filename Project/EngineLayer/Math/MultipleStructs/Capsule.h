#pragma once
#include "Vector3.h"
#include "Segment.h"

struct Capsule
{
	Segment segment; // 中心線分
	float radius = 0.1f;

	// 中心点（描画用）
	Vector3 GetCenter() const { return (segment.origin + segment.diff) * 0.5f; }

	// 高さ（球部分を除いたシリンダー長を含む全長）
	float GetHeight() const { return Vector3::Length(segment.diff - segment.origin); }

	// 軸ベクトル（正規化済み）
	Vector3 GetAxis() const { return Vector3::Normalize(segment.diff - segment.origin); }
};
