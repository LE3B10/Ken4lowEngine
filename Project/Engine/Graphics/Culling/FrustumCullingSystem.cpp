#include "FrustumCullingSystem.h"

namespace Ken4lowEngine
{
	void FrustumCullingSystem::BuildFrustum(const Matrix4x4& viewProjection)
	{
		viewProjection_ = viewProjection;
		frustum_.BuildFromViewProjection(viewProjection_);
	}

	void FrustumCullingSystem::ResetStatistics()
	{
		totalObjectCount_ = 0;
		drawnObjectCount_ = 0;
		culledObjectCount_ = 0;
	}

	bool FrustumCullingSystem::IsVisible(const BoundingSphere& bounds, bool ignoreFrustumCulling)
	{
		return IsVisibleInternal(frustum_.Intersects(bounds), ignoreFrustumCulling);
	}

	bool FrustumCullingSystem::IsVisible(const BoundingAABB& bounds, bool ignoreFrustumCulling)
	{
		return IsVisibleInternal(frustum_.Intersects(bounds), ignoreFrustumCulling);
	}

	bool FrustumCullingSystem::IsVisibleInternal(bool intersects, bool ignoreFrustumCulling)
	{
		++totalObjectCount_;

		if (!enabled_ || ignoreFrustumCulling || intersects)
		{
			++drawnObjectCount_;
			return true;
		}

		++culledObjectCount_;
		return false;
	}
}
