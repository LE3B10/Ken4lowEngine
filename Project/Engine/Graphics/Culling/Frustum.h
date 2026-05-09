#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <array>

namespace Ken4lowEngine
{
	/// <summary>
	/// ローカル/ワールド空間で扱う簡易的な球 Bounds。
	/// </summary>
	struct BoundingSphere
	{
		Vector3 center{};
		float radius = 1.0f;
	};

	/// <summary>
	/// ViewProjection 行列から生成した 6 平面で可視判定を行う視錐台。
	/// </summary>
	class Frustum
	{
	public:
		void BuildFromViewProjection(const Matrix4x4& viewProjection);
		bool Intersects(const BoundingSphere& sphere) const;

	private:
		void NormalizePlane(Vector4& plane) const;

		std::array<Vector4, 6> planes_{};
	};
}
