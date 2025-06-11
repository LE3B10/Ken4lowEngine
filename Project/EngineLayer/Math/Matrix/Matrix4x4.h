#pragma once
#include <cmath>
#include "assimp/matrix4x4.h"

class Vector3;
class Quaternion;

/// <summary>
/// 4x4行列
/// </summary>
class Matrix4x4 final
{
public:

	// 平行移動成分を取得する関数を追加
	Vector3 GetTranslation() const;

	float m[4][4];

	// デフォルトコンストラクタ
	Matrix4x4();

	// 指定された値で初期化するコンストラクタ
	Matrix4x4(float elements[4][4]);

	// 要素ごとに初期化するコンストラクタ
	Matrix4x4(
		float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23, float m30, float m31, float m32, float m33) {
		m[0][0] = m00;
		m[0][1] = m01;
		m[0][2] = m02;
		m[0][3] = m03;
		m[1][0] = m10;
		m[1][1] = m11;
		m[1][2] = m12;
		m[1][3] = m13;
		m[2][0] = m20;
		m[2][1] = m21;
		m[2][2] = m22;
		m[2][3] = m23;
		m[3][0] = m30;
		m[3][1] = m31;
		m[3][2] = m32;
		m[3][3] = m33;
	}

	Matrix4x4& operator+=(const Matrix4x4& other);
	Matrix4x4& operator-=(const Matrix4x4& other);
	Matrix4x4& operator*=(const Matrix4x4& other);
	Matrix4x4& operator=(const Matrix4x4& other);

	friend Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2);
	friend Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2);
	friend Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2);


	// 行列の加法
	static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

	// 行列の減法
	static Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);

	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& matrix);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4& m);

	// 単位行列
	static Matrix4x4 MakeIdentity();

	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// X軸の回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);

	// Y軸の回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);

	// Z軸の回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// XYZ回転行列
	static Matrix4x4 MakeRotateMatrix(const Vector3& radian);

	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// 三次元アフィン変換行列（Vector3）
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// アフィン変換行列（Quaternion）
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	// 透視投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	// 
	static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);
};
