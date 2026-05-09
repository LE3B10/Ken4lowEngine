#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <array>

namespace Ken4lowEngine
{
	/// <summary>
	/// ViewProjection 逆行列から視錐台 8 頂点を復元し、Wireframe で描画するデバッグ補助。
	/// </summary>
	class FrustumDebugRenderer
	{
	public:
		std::array<Vector3, 8> BuildCorners(const Matrix4x4& viewProjection) const;
		void Draw(const Matrix4x4& viewProjection, const Vector4& color) const;
	};
}
