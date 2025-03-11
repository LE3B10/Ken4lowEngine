#pragma once
#include "Matrix4x4.h"


class Vector4
{
public: /// ---------- メンバ変数 ---------- ///
	float x, y, z, w;

	// Vector4をMatrix4x4で変換する関数
	static Vector4 Transform(const Vector4& vector, const Matrix4x4& matrix);
};