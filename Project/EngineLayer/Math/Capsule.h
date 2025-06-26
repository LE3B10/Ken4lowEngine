#pragma once
#include "Vector3.h"

struct Capsule 
{
	Vector3 pointA;  // 始点（下端球の中心）
	Vector3 pointB;  // 終点（上端球の中心）
	float radius = 0.1f;

	// 中心点（描画用）
	Vector3 GetCenter() const { return (pointA + pointB) * 0.5f; }

	// 高さ（球部分を除いたシリンダー長を含む全長）
	float GetHeight() const { return Vector3::Length(pointB - pointA); }

	// 軸ベクトル（正規化済み）
	Vector3 GetAxis() const { return Vector3::Normalize(pointB - pointA); }
};
