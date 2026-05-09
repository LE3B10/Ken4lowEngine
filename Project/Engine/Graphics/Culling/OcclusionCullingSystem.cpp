#define NOMINMAX
#include "OcclusionCullingSystem.h"

#include "StageChunk.h"
#include "Wireframe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kClipEpsilon = 1.0e-4f;
		constexpr float kMinOccluderHeight = 2.0f;
		constexpr float kMinOccluderLongSide = 4.0f;
		constexpr float kMaxWallThickness = 3.0f;
		constexpr float kMinOccluderBoxSide = 4.0f;

		BoundingAABB ToBoundingAABB(const AABB& aabb)
		{
			return { aabb.min, aabb.max };
		}

		Vector3 GetSize(const BoundingAABB& bounds)
		{
			return bounds.max - bounds.min;
		}
	}

	void OcclusionCullingSystem::ClearOccluders()
	{
		occluders_.clear();
		statistics_ = {};
	}

	void OcclusionCullingSystem::AddOccluder(const Occluder& occluder)
	{
		occluders_.push_back(occluder);
	}

	void OcclusionCullingSystem::BuildAutoOccludersFromWorldAABBs(const std::vector<AABB>& worldAABBs)
	{
		ClearOccluders();
		for (const AABB& aabb : worldAABBs)
		{
			const BoundingAABB bounds = ToBoundingAABB(aabb);
			const Vector3 size = GetSize(bounds);
			const float longSide = std::max(size.x, size.z);
			const float shortSide = std::min(size.x, size.z);
			const bool wallLike = longSide >= kMinOccluderLongSide && shortSide <= kMaxWallThickness;
			const bool boxLike = size.x >= kMinOccluderBoxSide && size.y >= kMinOccluderHeight && size.z >= kMinOccluderBoxSide;
			if (size.y < kMinOccluderHeight || (!wallLike && !boxLike))
			{
				continue;
			}

			AddOccluder(Occluder(bounds, "AutoWallOccluder"));
		}
		statistics_.occluderCount = static_cast<int>(occluders_.size());
	}

	void OcclusionCullingSystem::ApplyToStageChunks(std::vector<StageChunk>& chunks, const Matrix4x4& viewProjection)
	{
		statistics_ = {};
		statistics_.occluderCount = static_cast<int>(occluders_.size());

		for (StageChunk& chunk : chunks)
		{
			chunk.SetOccludedByOcclusion(false);
			if (!chunk.IsVisible()) { continue; }
			++statistics_.testedChunkCount;

			// Frustum/StageChunk 後に、完全に遮蔽物へ隠れた Chunk の Draw だけを安全側で止める。
			if (enabled_ && IsOccluded(chunk.GetBounds(), viewProjection))
			{
				chunk.SetOccludedByOcclusion(true);
				chunk.SetVisible(false);
				++statistics_.occludedChunkCount;
			}
		}
	}

	void OcclusionCullingSystem::DrawDebugBounds() const
	{
		if (!showOccluderBounds_) { return; }

		const Vector4 occluderColor{ 1.0f, 0.7f, 0.05f, 1.0f };
		for (const Occluder& occluder : occluders_)
		{
			if (!occluder.IsEnabled()) { continue; }
			Wireframe::GetInstance()->DrawAABB(occluder.GetDebugAABB(), occluderColor);
		}
	}

	void OcclusionCullingSystem::SetCoverageThreshold(float threshold)
	{
		coverageThreshold_ = std::clamp(threshold, 0.0f, 1.0f);
	}

	void OcclusionCullingSystem::SetDepthBias(float depthBias)
	{
		depthBias_ = std::max(0.0f, depthBias);
	}

	void OcclusionCullingSystem::SetOcclusionMargin(float margin)
	{
		occlusionMargin_ = std::max(0.0f, margin);
	}

	bool OcclusionCullingSystem::IsOccluded(const BoundingAABB& bounds, const Matrix4x4& viewProjection) const
	{
		ScreenRect targetRect{};
		if (!ProjectAABB(bounds, viewProjection, targetRect))
		{
			return false;
		}

		for (const Occluder& occluder : occluders_)
		{
			if (!occluder.IsEnabled()) { continue; }

			ScreenRect occluderRect{};
			if (!ProjectAABB(occluder.GetWorldBounds(), viewProjection, occluderRect))
			{
				continue;
			}

			if (targetRect.minDepth <= occluderRect.minDepth + depthBias_)
			{
				continue;
			}

			ScreenRect safeOccluderRect = occluderRect;
			safeOccluderRect.minX += occlusionMargin_;
			safeOccluderRect.maxX -= occlusionMargin_;
			safeOccluderRect.minY += occlusionMargin_;
			safeOccluderRect.maxY -= occlusionMargin_;
			if (safeOccluderRect.minX >= safeOccluderRect.maxX || safeOccluderRect.minY >= safeOccluderRect.maxY)
			{
				continue;
			}

			if (CalculateCoverage(safeOccluderRect, targetRect) >= coverageThreshold_)
			{
				return true;
			}
		}

		return false;
	}

	bool OcclusionCullingSystem::ProjectAABB(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ScreenRect& outRect) const
	{
		const std::array<Vector3, 8> corners = {
			Vector3{ bounds.min.x, bounds.min.y, bounds.min.z },
			Vector3{ bounds.max.x, bounds.min.y, bounds.min.z },
			Vector3{ bounds.min.x, bounds.max.y, bounds.min.z },
			Vector3{ bounds.max.x, bounds.max.y, bounds.min.z },
			Vector3{ bounds.min.x, bounds.min.y, bounds.max.z },
			Vector3{ bounds.max.x, bounds.min.y, bounds.max.z },
			Vector3{ bounds.min.x, bounds.max.y, bounds.max.z },
			Vector3{ bounds.max.x, bounds.max.y, bounds.max.z },
		};

		outRect.minX = std::numeric_limits<float>::max();
		outRect.minY = std::numeric_limits<float>::max();
		outRect.maxX = std::numeric_limits<float>::lowest();
		outRect.maxY = std::numeric_limits<float>::lowest();
		outRect.minDepth = std::numeric_limits<float>::max();

		for (const Vector3& corner : corners)
		{
			const float clipX = corner.x * viewProjection.m[0][0] + corner.y * viewProjection.m[1][0] + corner.z * viewProjection.m[2][0] + viewProjection.m[3][0];
			const float clipY = corner.x * viewProjection.m[0][1] + corner.y * viewProjection.m[1][1] + corner.z * viewProjection.m[2][1] + viewProjection.m[3][1];
			const float clipZ = corner.x * viewProjection.m[0][2] + corner.y * viewProjection.m[1][2] + corner.z * viewProjection.m[2][2] + viewProjection.m[3][2];
			const float clipW = corner.x * viewProjection.m[0][3] + corner.y * viewProjection.m[1][3] + corner.z * viewProjection.m[2][3] + viewProjection.m[3][3];
			if (clipW <= kClipEpsilon)
			{
				return false;
			}

			const float invW = 1.0f / clipW;
			const float ndcX = clipX * invW;
			const float ndcY = clipY * invW;
			const float ndcZ = clipZ * invW;
			if (ndcZ < 0.0f || ndcZ > 1.0f)
			{
				return false;
			}

			const float screenX = ndcX * 0.5f + 0.5f;
			const float screenY = -ndcY * 0.5f + 0.5f;
			outRect.minX = std::min(outRect.minX, screenX);
			outRect.maxX = std::max(outRect.maxX, screenX);
			outRect.minY = std::min(outRect.minY, screenY);
			outRect.maxY = std::max(outRect.maxY, screenY);
			outRect.minDepth = std::min(outRect.minDepth, ndcZ);
		}

		outRect.minX = std::clamp(outRect.minX, 0.0f, 1.0f);
		outRect.maxX = std::clamp(outRect.maxX, 0.0f, 1.0f);
		outRect.minY = std::clamp(outRect.minY, 0.0f, 1.0f);
		outRect.maxY = std::clamp(outRect.maxY, 0.0f, 1.0f);
		return outRect.minX < outRect.maxX && outRect.minY < outRect.maxY;
	}

	float OcclusionCullingSystem::CalculateCoverage(const ScreenRect& occluderRect, const ScreenRect& targetRect)
	{
		const float targetWidth = targetRect.maxX - targetRect.minX;
		const float targetHeight = targetRect.maxY - targetRect.minY;
		const float targetArea = targetWidth * targetHeight;
		if (targetArea <= 0.0f)
		{
			return 0.0f;
		}

		const float overlapMinX = std::max(occluderRect.minX, targetRect.minX);
		const float overlapMaxX = std::min(occluderRect.maxX, targetRect.maxX);
		const float overlapMinY = std::max(occluderRect.minY, targetRect.minY);
		const float overlapMaxY = std::min(occluderRect.maxY, targetRect.maxY);
		const float overlapWidth = std::max(0.0f, overlapMaxX - overlapMinX);
		const float overlapHeight = std::max(0.0f, overlapMaxY - overlapMinY);
		return (overlapWidth * overlapHeight) / targetArea;
	}
}
