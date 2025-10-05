#pragma once

/// <summary>
/// 2次元ベクトル
/// </summary>
class Vector2 final
{
public: /// ---------- メンバ変数 ---------- ///
	float x, y;

public: /// ---------- 算術演算子 ---------- ///

	// 加算
	Vector2 operator+(const Vector2& other);
	// 減算
	Vector2 operator-(const Vector2& other);
	// 乗算
	Vector2 operator*(const Vector2& other);
	// 除算
	Vector2 operator/(const Vector2& other);

	// +=
	Vector2 operator+=(const Vector2& v);
	// -=
	Vector2 operator-=(const Vector2& v);
	// *= (スカラー倍)
	Vector2 operator*=(float scalar);
};