#pragma once

#include <AnimationModel.h>

#include <memory>
#include <vector>

/// <summary>
/// この処理は実ゲームではなく、DebugSceneでAnimationModelの大量描画負荷を確認するための検証用です。
/// </summary>
class AnimationModelBatchTest
{
public:
	AnimationModelBatchTest();
	~AnimationModelBatchTest();

	struct PerformanceStats
	{
		size_t modelCount = 0;
		size_t visibleCount = 0;
		size_t culledCount = 0;
		size_t drawSkinnedCount = 0;
		size_t dispatchSkinningCSCount = 0;
		size_t poseDirtyCount = 0;
		size_t paletteUpdateSkippedCount = 0;
		size_t computeSkinningSkippedCount = 0;
		size_t lastForcePoseUpdateCount = 0;
		size_t sharedPoseUpdateCount = 0;
		size_t sharedPaletteUpdateCount = 0;
		size_t perModelPaletteUpdateCount = 0;
		size_t paletteShareSavedCount = 0;
		size_t sharedPaletteDispatchCount = 0;
		float updateMilliseconds = 0.0f;
		float worldTransformMilliseconds = 0.0f;
		float animationTimeMilliseconds = 0.0f;
		float skeletonMilliseconds = 0.0f;
		float paletteMilliseconds = 0.0f;
		float computeSkinningMilliseconds = 0.0f;
		float drawMilliseconds = 0.0f;
		float playAnimationTimeSeconds = 0.0f;
		float animationDurationSeconds = 0.0f;
		float sharedSkeletonMilliseconds = 0.0f;
		float sharedPaletteMilliseconds = 0.0f;
		float perModelPaletteMilliseconds = 0.0f;
		float paletteShareSavedMilliseconds = 0.0f;
		bool animationLoaded = false;
		bool sharedPaletteValid = false;
		bool representativeOnlyFallback = false;
		std::vector<size_t> lodCounts{};
	};

	void Initialize();
	void Finalize();
	void Update(float deltaTime);
	void Draw();
	void DrawImGui();

private:
	void RebuildModelsSafely();
	void CreateTestModels();
	void RefreshVisibilityStats();
	void ApplyRuntimeSettings();

	std::vector<std::unique_ptr<Ken4lowEngine::AnimationModel>> models_{};
	PerformanceStats stats_{};
	bool enabled_ = false;
	int modelCountIndex_ = 2;
	float spacing_ = 2.5f;
	bool playAnimation_ = true;
	bool useComputeSkinning_ = true;
	bool useDrawBatched_ = true;
	bool useSharedAnimationPose_ = false;
	bool poseDirty_ = true;
	bool forcePoseUpdateRequest_ = false;
	bool previousPlayAnimation_ = true;
	bool rebuildRequested_ = false;
};
