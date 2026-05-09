#define NOMINMAX
#include "StageChunkManager.h"

#include "Object3DCommon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kMinChunkSize = 1.0f;
		constexpr float kBoundsPaddingY = 1.0f;

		float ClampChunkSize(float chunkSize)
		{
			return std::max(chunkSize, kMinChunkSize);
		}
	}

	void StageChunkManager::Clear()
	{
		chunks_.clear();
		statistics_ = {};
		needsRebuild_ = false;
	}

	void StageChunkManager::Rebuild(Object3D* stageObject, float chunkSize)
	{
		chunks_.clear();
		chunkSize_ = ClampChunkSize(chunkSize);
		needsRebuild_ = false;

		if (!stageObject || stageObject->GetSubmeshCount() == 0)
		{
			BuildStatistics();
			return;
		}

		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		struct PendingMesh
		{
			size_t meshIndex = 0;
			int xIndex = 0;
			int zIndex = 0;
		};
		std::vector<PendingMesh> pendingMeshes;
		pendingMeshes.reserve(stageObject->GetSubmeshCount());

		for (size_t meshIndex = 0; meshIndex < stageObject->GetSubmeshCount(); ++meshIndex)
		{
			const BoundingSphere meshBounds = stageObject->GetMeshWorldBoundsForCulling(meshIndex);
			if (!stageObject->HasMeshWorldBoundsForCulling(meshIndex))
			{
				continue;
			}

			const int xIndex = static_cast<int>(std::floor(meshBounds.center.x / chunkSize_));
			const int zIndex = static_cast<int>(std::floor(meshBounds.center.z / chunkSize_));
			pendingMeshes.push_back({ meshIndex, xIndex, zIndex });
			minY = std::min(minY, meshBounds.center.y - meshBounds.radius);
			maxY = std::max(maxY, meshBounds.center.y + meshBounds.radius);
		}

		if (pendingMeshes.empty())
		{
			BuildStatistics();
			return;
		}

		minY -= kBoundsPaddingY;
		maxY += kBoundsPaddingY;
		const float height = std::max(maxY - minY, kMinChunkSize);
		const float centerY = (minY + maxY) * 0.5f;

		std::unordered_map<unsigned long long, size_t> keyToChunkIndex;
		for (const PendingMesh& pending : pendingMeshes)
		{
			const unsigned long long key = CalculateChunkKey(pending.xIndex, pending.zIndex);
			auto it = keyToChunkIndex.find(key);
			if (it == keyToChunkIndex.end())
			{
				const int chunkId = static_cast<int>(chunks_.size());
				const Vector3 center = {
					(static_cast<float>(pending.xIndex) + 0.5f) * chunkSize_,
					centerY,
					(static_cast<float>(pending.zIndex) + 0.5f) * chunkSize_
				};
				const Vector3 size = { chunkSize_, height, chunkSize_ };
				it = keyToChunkIndex.emplace(key, chunks_.size()).first;
				chunks_.emplace_back(chunkId, center, size);
			}

			chunks_[it->second].AddMesh(stageObject, pending.meshIndex);
		}

		BuildStatistics();
	}

	void StageChunkManager::UpdateVisibility(bool enabled)
	{
		statistics_.drawnChunkCount = 0;
		statistics_.culledChunkCount = 0;

		auto& cullingSystem = Object3DCommon::GetInstance()->GetFrustumCullingSystem();
		for (StageChunk& chunk : chunks_)
		{
			// 既存 FrustumCullingSystem に Chunk AABB を渡し、Chunk 外の静的 Mesh Draw だけを止める。
			const bool visible = cullingSystem.IsVisible(
				chunk.GetBounds(),
				!enabled,
				true,
				FrustumCullingSystem::CullingUnit::StageChunk);
			chunk.SetVisible(visible);
			if (visible)
			{
				++statistics_.drawnChunkCount;
			}
			else
			{
				++statistics_.culledChunkCount;
			}
		}
	}

	void StageChunkManager::DrawVisibleChunks() const
	{
		for (const StageChunk& chunk : chunks_)
		{
			chunk.Draw();
		}
	}

	void StageChunkManager::DrawDebugBounds() const
	{
		for (const StageChunk& chunk : chunks_)
		{
			chunk.DrawBoundsDebug(showBounds_);
		}
	}

	void StageChunkManager::BuildStatistics()
	{
		statistics_ = {};
		statistics_.totalChunkCount = static_cast<int>(chunks_.size());
		for (const StageChunk& chunk : chunks_)
		{
			statistics_.totalObjectCountInChunks += chunk.GetObjectCount();
			statistics_.totalMeshCountInChunks += static_cast<int>(chunk.GetMeshes().size());
		}
	}

	unsigned long long StageChunkManager::CalculateChunkKey(int xIndex, int zIndex) const
	{
		return (static_cast<unsigned long long>(static_cast<unsigned int>(xIndex)) << 32) | static_cast<unsigned int>(zIndex);
	}
}
