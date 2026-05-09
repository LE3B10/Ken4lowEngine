#pragma once
#include "BoundingVolume.h"
#include "Frustum.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Scene や ImGui に依存しない Frustum Culling の実行状態と統計。
	/// </summary>
	class FrustumCullingSystem
	{
	public:
		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }

		void BuildFrustum(const Matrix4x4& viewProjection);
		void ResetStatistics();

		bool IsVisible(const BoundingSphere& bounds, bool ignoreFrustumCulling = false);
		bool IsVisible(const BoundingAABB& bounds, bool ignoreFrustumCulling = false);

		int GetTotalObjectCount() const { return totalObjectCount_; }
		int GetDrawnObjectCount() const { return drawnObjectCount_; }
		int GetCulledObjectCount() const { return culledObjectCount_; }

		const Matrix4x4& GetViewProjectionMatrix() const { return viewProjection_; }
		const Frustum& GetFrustum() const { return frustum_; }

	private:
		bool IsVisibleInternal(bool intersects, bool ignoreFrustumCulling);

		Frustum frustum_{};
		Matrix4x4 viewProjection_{};
		bool enabled_ = false;
		int totalObjectCount_ = 0;
		int drawnObjectCount_ = 0;
		int culledObjectCount_ = 0;
	};
}
