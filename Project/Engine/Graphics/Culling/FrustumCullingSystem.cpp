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
		cullingDisabledDrawnObjectCount_ = 0;
		missingBoundsDrawnObjectCount_ = 0;
	}

	bool FrustumCullingSystem::IsVisible(const BoundingSphere& bounds, bool ignoreFrustumCulling, bool hasBounds)
	{
		return IsVisibleInternal(hasBounds ? frustum_.Intersects(bounds) : true, ignoreFrustumCulling, hasBounds);
	}

	bool FrustumCullingSystem::IsVisible(const BoundingAABB& bounds, bool ignoreFrustumCulling, bool hasBounds)
	{
		return IsVisibleInternal(hasBounds ? frustum_.Intersects(bounds) : true, ignoreFrustumCulling, hasBounds);
	}

	bool FrustumCullingSystem::IsVisibleInternal(bool intersects, bool ignoreFrustumCulling, bool hasBounds)
	{
		++totalObjectCount_;

		if (!enabled_ || ignoreFrustumCulling)
		{
			++drawnObjectCount_;
			++cullingDisabledDrawnObjectCount_;
			return true;
		}

		if (!hasBounds)
		{
			++drawnObjectCount_;
			++missingBoundsDrawnObjectCount_;
			return true;
		}

		if (intersects)
		{
			++drawnObjectCount_;
			return true;
		}

		++culledObjectCount_;
		return false;
	}
}
