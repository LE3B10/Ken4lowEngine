#pragma once
#include "BoundingVolume.h"
#include "Matrix4x4.h"
#include "Occluder.h"

#include <vector>

namespace Ken4lowEngine
{
	class StageChunk;

	/// <summary>
	/// Occluder と StageChunk Bounds をスクリーン矩形に投影して判定する CPU 側の簡易 Occlusion Culling。
	/// </summary>
	class OcclusionCullingSystem
	{
	public:
		struct Statistics
		{
			int occluderCount = 0;
			int testedChunkCount = 0;
			int occludedChunkCount = 0;
		};

		void ClearOccluders();
		void AddOccluder(const Occluder& occluder);
		void BuildAutoOccludersFromWorldAABBs(const std::vector<AABB>& worldAABBs);
		void ApplyToStageChunks(std::vector<StageChunk>& chunks, const Matrix4x4& viewProjection);
		void DrawDebugBounds() const;

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetShowOccluderBounds(bool visible) { showOccluderBounds_ = visible; }
		bool IsShowOccluderBounds() const { return showOccluderBounds_; }
		void SetShowOccludedBounds(bool visible) { showOccludedBounds_ = visible; }
		bool IsShowOccludedBounds() const { return showOccludedBounds_; }
		void SetCoverageThreshold(float threshold);
		float GetCoverageThreshold() const { return coverageThreshold_; }
		void SetDepthBias(float depthBias);
		float GetDepthBias() const { return depthBias_; }
		void SetOcclusionMargin(float margin);
		float GetOcclusionMargin() const { return occlusionMargin_; }
		const Statistics& GetStatistics() const { return statistics_; }
		const std::vector<Occluder>& GetOccluders() const { return occluders_; }

	private:
		struct ScreenRect
		{
			float minX = 0.0f;
			float maxX = 0.0f;
			float minY = 0.0f;
			float maxY = 0.0f;
			float minDepth = 1.0f;
		};

		bool IsOccluded(const BoundingAABB& bounds, const Matrix4x4& viewProjection) const;
		bool ProjectAABB(const BoundingAABB& bounds, const Matrix4x4& viewProjection, ScreenRect& outRect) const;
		static float CalculateCoverage(const ScreenRect& occluderRect, const ScreenRect& targetRect);

		std::vector<Occluder> occluders_{};
		Statistics statistics_{};
		bool enabled_ = false;
		bool showOccluderBounds_ = false;
		bool showOccludedBounds_ = false;
		float coverageThreshold_ = 0.98f;
		float depthBias_ = 0.02f;
		float occlusionMargin_ = 0.02f;
	};
}
