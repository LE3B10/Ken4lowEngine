#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

class Quaternion
{
public:

	float x, y, z, w;

	// Quaternionの積
	static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);
	
	// 単位Quaternionを返す
	static Quaternion IdentityQuaternion();
	
	// 共役Quaternionを返す
	static Quaternion Conjugate(const Quaternion& quaternion);
	
	// QuaternionのNormを返す
	static float Norm(const Quaternion& quaternion);
	
	// 正規化したQuaternionを返す
	static Quaternion Normalize(const Quaternion& quaternion);
	
	// 逆Quaternionを返す
	static Quaternion Inverse(const Quaternion& quaternion);
	
	// 任意軸回転を表すQuaternionの生成
	static Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle);
	
	// ベクトルをQuaternionで回転させた結果のベクトルを求める
	static Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion);
	
	// Quaternionから回転行列を求める
	static Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);
	
	// 球面線形補間
	static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);
};
