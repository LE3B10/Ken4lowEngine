#pragma once
#include "StageChunk.h"

#include <vector>

namespace Ken4lowEngine
{
	class StageChunkManager
	{
	public:
		struct Statistics
		{
			int totalChunkCount = 0;
			int drawnChunkCount = 0;
			int culledChunkCount = 0;
			int totalObjectCountInChunks = 0;
			int totalMeshCountInChunks = 0;
		};

		void Clear();
		void Rebuild(Object3D* stageObject, float chunkSize);
		void UpdateVisibility(bool enabled);
		void DrawVisibleChunks() const;
		void DrawDebugBounds() const;

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetShowBounds(bool showBounds) { showBounds_ = showBounds; }
		bool IsShowBounds() const { return showBounds_; }
		void SetChunkSize(float chunkSize) { chunkSize_ = chunkSize; }
		float GetChunkSize() const { return chunkSize_; }
		bool NeedsRebuild() const { return needsRebuild_; }
		void MarkRebuildRequested() { needsRebuild_ = true; }

		const Statistics& GetStatistics() const { return statistics_; }
		const std::vector<StageChunk>& GetChunks() const { return chunks_; }

	private:
		void BuildStatistics();
		unsigned long long CalculateChunkKey(int xIndex, int zIndex) const;

		std::vector<StageChunk> chunks_{};
		Statistics statistics_{};
		float chunkSize_ = 20.0f;
		bool enabled_ = true;
		bool showBounds_ = false;
		bool needsRebuild_ = false;
	};
}
