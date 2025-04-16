#pragma once
#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstdint>

///==========================================================
/// マテリアルの拡張
///==========================================================
struct Material final
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
};
///==========================================================
/// マテリアルの拡張
///==========================================================