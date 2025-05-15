#pragma once
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <numbers>

class Matrix4x4;

/// <summary>
/// 3次元ベクトル
/// </summary>
class Vector3
{
public:
	float x, y, z;

	// 任意の共通ヘッダ（例：MathUtil.h）に置くと便利
	static inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	static inline float LerpAngle(float a, float b, float t)
	{
		a = NormalizeAngle(a);
		b = NormalizeAngle(b);

		float diff = b - a;

		// 最短方向へ補間（例: -179° → +179°）
		if (diff > std::numbers::pi_v<float>)
			diff -= 2.0f * std::numbers::pi_v<float>;
		else if (diff < -std::numbers::pi_v<float>)
			diff += 2.0f * std::numbers::pi_v<float>;

		return a + diff * t;
	}

	static inline float NormalizeAngle(float angle)
	{
		// -π 〜 +π に収める
		angle = std::fmod(angle + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
		if (angle < 0) angle += 2.0f * std::numbers::pi_v<float>;
		return angle - std::numbers::pi_v<float>;
	}


	Vector3() : x(0), y(0), z(0) {};
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {};

	//加算
	static Vector3 Add(const Vector3& v1, const Vector3& v2);

	//減算
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	//スカラー倍
	static Vector3 Multiply(float scalar, const Vector3& v);

	//内積
	static float Dot(const Vector3& v1, const Vector3& v2);

	//長さ（ノルム）
	static float Length(const Vector3& v);

	//正規化
	static Vector3 Normalize(const Vector3& v);

	//座標変換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	//クロス積
	static Vector3 Cross(const Vector3& v1, const Vector3& v2);

	static Vector3 CatmullRomSpline(const Vector3& P0, const Vector3& P1, const Vector3& P2, const Vector3& P3, float t);

	static Vector3 Lerp(const Vector3& start, const Vector3& end, float t);

	Vector3 operator+() const;
	Vector3 operator-() const;
	Vector3& operator+=(const Vector3& other);
	Vector3& operator-=(const Vector3& other);
	Vector3& operator*=(float s);
	Vector3& operator/=(float s);

	friend Vector3 operator+(const Vector3& v1, const Vector3& v2);
	friend Vector3 operator-(const Vector3& v1, const Vector3& v2);
	friend Vector3 operator*(const Vector3& v1, const Vector3& v2);
	friend Vector3 operator*(const Vector3& v, float s);
	friend Vector3 operator*(float s, const Vector3& v);
	friend Vector3 operator/(const Vector3& v, float s);
	friend Vector3 operator*(const Matrix4x4& matrix, const Vector3& vec);

	// 等価演算子
	bool operator==(const Vector3& other) const;
	bool operator!=(const Vector3& other) const;

	// [] 演算子のオーバーロード（読み取り用）
	float operator[](int index) const {
		switch (index) {
		case 0:
			return x;
		case 1:
			return y;
		case 2:
			return z;
		default:
			throw std::out_of_range("Index out of range");
		}
	}

	// [] 演算子のオーバーロード（書き込み用）
	float& operator[](int index) {
		switch (index) {
		case 0:
			return x;
		case 1:
			return y;
		case 2:
			return z;
		default:
			throw std::out_of_range("Index out of range");
		}
	}
};
