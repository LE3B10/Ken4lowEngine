#include "FrustumDebugRenderer.h"

#include "Wireframe.h"

namespace Ken4lowEngine
{
	std::array<Vector3, 8> FrustumDebugRenderer::BuildCorners(const Matrix4x4& viewProjection) const
	{
		const Matrix4x4 inverseViewProjection = Matrix4x4::Inverse(viewProjection);
		const std::array<Vector3, 8> ndcCorners = {
			Vector3{ -1.0f, -1.0f, 0.0f },
			Vector3{ -1.0f,  1.0f, 0.0f },
			Vector3{  1.0f,  1.0f, 0.0f },
			Vector3{  1.0f, -1.0f, 0.0f },
			Vector3{ -1.0f, -1.0f, 1.0f },
			Vector3{ -1.0f,  1.0f, 1.0f },
			Vector3{  1.0f,  1.0f, 1.0f },
			Vector3{  1.0f, -1.0f, 1.0f },
		};

		std::array<Vector3, 8> worldCorners{};
		for (std::size_t i = 0; i < ndcCorners.size(); ++i)
		{
			worldCorners[i] = Vector3::Transform(ndcCorners[i], inverseViewProjection);
		}
		return worldCorners;
	}

	void FrustumDebugRenderer::Draw(const Matrix4x4& viewProjection, const Vector4& color) const
	{
		Wireframe::GetInstance()->DrawFrustum(BuildCorners(viewProjection), color);
	}
}
