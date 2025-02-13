#pragma once
#include <stdexcept>
#include <cassert>

class Matrix4x4;

/// <summary>
/// 3次元ベクトル
/// </summary>
class Vector3 {
public:
	float x, y, z;

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
