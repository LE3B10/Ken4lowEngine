#pragma once

/// -------------------------------------------------------------
///							2次元ベクトル
/// -------------------------------------------------------------
class Vector2 final
{
public: /// ---------- メンバ変数 ---------- ///

	// 成分
	float x, y;

public:	/// ---------- 二項演算子 ---------- ///

	Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; }; // 和
	Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; }; // 差
	Vector2 operator*(const Vector2& other) const { return { x * other.x, y * other.y }; }; // 積
	Vector2 operator/(const Vector2& other) const { return { x / other.x, y / other.y }; }; // 商

public:	/// ---------- スカラー演算子 ---------- ///

	Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; } // スカラー倍
	Vector2 operator/(float scalar) const { return { x / scalar, y / scalar }; } // スカラー除算

public:	/// ---------- 複合代入演算子（参照返し） ---------- ///

	Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; } // 和
	Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; } // 差
	Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; } // スカラー倍
	Vector2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; } // スカラー除算

public: /// ---------- 等価演算子 ---------- ///

	bool operator==(const Vector2& other) const { return x == other.x && y == other.y; } // 等価
	bool operator!=(const Vector2& other) const { return !(*this == other); } // 非等価

public: /// ---------- 単項演算子 ---------- ///

	Vector2 operator+() const { return *this; } // 単項プラス
	Vector2 operator-() const { return { -x, -y }; } // 単項マイナス
	float operator[](int index) const { return (&x)[index]; } // 添字演算子（読み取り専用）
	float& operator[](int index) { return (&x)[index]; } // 添字演算子（書き込み可能）
};

// 非メンバ
inline Vector2 operator*(float scalar, const Vector2& v) { return v * scalar; } // スカラー倍（スカラーが左側）