#define NOMINMAX
#include "AnimationModelBatchTest.h"

#include <AnimationModel.h>
#include <DirectXCommon.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	constexpr int kModelCounts[] = { 1, 5, 10, 20, 30, 50 };
	constexpr const char* kModelCountLabels[] = { "1", "5", "10", "20", "30", "50" };
	constexpr const char* kTestModelPath = "Animation/humanWalking.gltf";
}

AnimationModelBatchTest::AnimationModelBatchTest() = default;
AnimationModelBatchTest::~AnimationModelBatchTest()
{
	Finalize();
}

void AnimationModelBatchTest::Initialize()
{
	stats_ = {};
}

void AnimationModelBatchTest::Finalize()
{
	// UAVディスクリプタを含むAnimationModel固有リソースを、DebugScene終了時に明示解放する。
	for (auto& model : models_)
	{
		if (model) { model->Clear(); }
	}
	models_.clear();
	stats_ = {};
}

void AnimationModelBatchTest::Update(float deltaTime)
{
	if (!enabled_) { return; }
	if (models_.empty()) { rebuildRequested_ = true; }
	if (rebuildRequested_)
	{
		// ボタン押下フレームの描画完了後、次UpdateでのみGPU待機を伴う再生成を行う。
		RebuildModelsSafely();
		rebuildRequested_ = false;
		forcePoseUpdateRequest_ = true;
	}

	ApplyRuntimeSettings();
	const auto begin = std::chrono::steady_clock::now();
	stats_.worldTransformMilliseconds = 0.0f;
	stats_.animationTimeMilliseconds = 0.0f;
	stats_.skeletonMilliseconds = 0.0f;
	stats_.paletteMilliseconds = 0.0f;
	stats_.poseDirtyCount = 0;
	stats_.paletteUpdateSkippedCount = 0;
	stats_.sharedPoseUpdateCount = 0;
	stats_.sharedPaletteUpdateCount = 0;
	stats_.perModelPaletteUpdateCount = 0;
	stats_.paletteShareSavedCount = 0;
	stats_.sharedSkeletonMilliseconds = 0.0f;
	stats_.sharedPaletteMilliseconds = 0.0f;
	stats_.perModelPaletteMilliseconds = 0.0f;
	stats_.paletteShareSavedMilliseconds = 0.0f;
	stats_.sharedPaletteDispatchCount = 0;
	stats_.representativeOnlyFallback = false;
	const bool forceRequestThisFrame = forcePoseUpdateRequest_;
	const bool playStarted = playAnimation_ && !previousPlayAnimation_;
	const bool forcePoseUpdateThisFrame = forceRequestThisFrame || playStarted;
	Ken4lowEngine::AnimationModel* representative = models_.empty() ? nullptr : models_[0].get();
	stats_.sharedPaletteValid = useSharedAnimationPose_ && representative
		&& representative->GetCurrentPaletteSrvForDebugBatchTest().ptr != 0;
	const bool updateAllModelPoses = !useSharedAnimationPose_ || !stats_.sharedPaletteValid;
	size_t shareCompatibleFollowerCount = 0;
	for (size_t modelIndex = 0; modelIndex < models_.size(); ++modelIndex)
	{
		auto& model = models_[modelIndex];
		if (!model) { continue; }
		const bool isSharedSource = stats_.sharedPaletteValid && modelIndex == 0;
		const bool updateThisPose = updateAllModelPoses || isSharedSource;
		// 同じモデル・同じアニメーション時間のDebug用モデル群では、骨更新とPalette更新を代表1体に集約し、CPU側の更新負荷を検証する。
		auto timings = model->UpdateForDebugBatchTest(
			updateThisPose ? playAnimation_ : false,
			updateThisPose ? forcePoseUpdateThisFrame : false,
			updateThisPose ? deltaTime : 0.0f);

		const bool canUseSharedPalette = stats_.sharedPaletteValid && representative
			&& modelIndex > 0 && model->CanSharePaletteWithForDebugBatchTest(*representative);
		if (canUseSharedPalette)
		{
			++shareCompatibleFollowerCount;
		}
		else if (stats_.sharedPaletteValid && modelIndex > 0)
		{
			// LODなどが一致しない個体だけ、代表時刻へ同期して自前Paletteを更新する。
			model->SetAnimationTimeForDebugBatchTest(stats_.playAnimationTimeSeconds);
			const auto fallbackTimings = model->UpdateForDebugBatchTest(false, true, 0.0f);
			timings.worldTransformMilliseconds += fallbackTimings.worldTransformMilliseconds;
			timings.skeletonMilliseconds += fallbackTimings.skeletonMilliseconds;
			timings.paletteMilliseconds += fallbackTimings.paletteMilliseconds;
			timings.poseUpdated = timings.poseUpdated || fallbackTimings.poseUpdated;
		}
		stats_.worldTransformMilliseconds += timings.worldTransformMilliseconds;
		stats_.animationTimeMilliseconds += timings.animationTimeMilliseconds;
		stats_.skeletonMilliseconds += timings.skeletonMilliseconds;
		stats_.paletteMilliseconds += timings.paletteMilliseconds;
		if (modelIndex == 0) { stats_.playAnimationTimeSeconds = timings.playAnimationTimeSeconds; }
		if (timings.poseUpdated)
		{
			++stats_.poseDirtyCount;
			if (isSharedSource)
			{
				++stats_.sharedPoseUpdateCount;
				++stats_.sharedPaletteUpdateCount;
				stats_.sharedSkeletonMilliseconds += timings.skeletonMilliseconds;
				stats_.sharedPaletteMilliseconds += timings.paletteMilliseconds;
			}
			else
			{
				++stats_.perModelPaletteUpdateCount;
				stats_.perModelPaletteMilliseconds += timings.paletteMilliseconds;
			}
		}
		else { ++stats_.paletteUpdateSkippedCount; }
	}
	if (stats_.sharedPaletteValid && stats_.sharedPaletteUpdateCount > 0)
	{
		stats_.paletteShareSavedCount = shareCompatibleFollowerCount;
		stats_.paletteShareSavedMilliseconds =
			stats_.sharedPaletteMilliseconds * static_cast<float>(stats_.paletteShareSavedCount);
	}
	if (forceRequestThisFrame) { stats_.lastForcePoseUpdateCount = stats_.poseDirtyCount; }
	if (stats_.poseDirtyCount > 0)
	{
		poseDirty_ = true;
	}
	forcePoseUpdateRequest_ = false;
	previousPlayAnimation_ = playAnimation_;
	stats_.updateMilliseconds = std::max(
		std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count(),
		0.000001f);
	RefreshVisibilityStats();
}

void AnimationModelBatchTest::Draw()
{
	if (!enabled_ || models_.empty()) { return; }
	// ImGui変更を同じ描画フレームへ反映し、Compute Skinning比較の状態ずれを防ぐ。
	ApplyRuntimeSettings();

	stats_.computeSkinningMilliseconds = 0.0f;
	stats_.dispatchSkinningCSCount = 0;
	stats_.computeSkinningSkippedCount = stats_.visibleCount;
	if (useComputeSkinning_ && poseDirty_)
	{
		const auto computeBegin = std::chrono::steady_clock::now();
		if (useSharedAnimationPose_)
		{
			const auto dispatchStats = Ken4lowEngine::AnimationModel::DispatchSkinningBatchedWithSharedPaletteForDebugTest(
				models_, models_.empty() ? nullptr : models_[0].get());
			stats_.sharedPaletteValid = dispatchStats.sharedPaletteValid;
			stats_.sharedPaletteDispatchCount = dispatchStats.sharedPaletteDispatchCount;
			stats_.dispatchSkinningCSCount = dispatchStats.sharedPaletteDispatchCount + dispatchStats.fallbackDispatchCount;
		}
		else
		{
			stats_.dispatchSkinningCSCount =
				Ken4lowEngine::AnimationModel::DispatchSkinningBatchedForDebugTest(models_);
			stats_.sharedPaletteDispatchCount = 0;
		}
		stats_.computeSkinningMilliseconds =
			std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - computeBegin).count();
		stats_.computeSkinningSkippedCount = stats_.visibleCount > stats_.dispatchSkinningCSCount
			? stats_.visibleCount - stats_.dispatchSkinningCSCount
			: 0;
		poseDirty_ = false;
	}

	// DrawBatched内部の通常Dispatchは止め、上で計測した必要フレームだけComputeを実行する。
	for (auto& model : models_)
	{
		if (model) { model->SetComputeSkinningEnabled(false); }
	}
	const auto begin = std::chrono::steady_clock::now();
	if (useDrawBatched_)
	{
		// DebugScene専用テストでは既存の一括パイプライン設定経路をそのまま比較する。
		Ken4lowEngine::AnimationModel::DrawBatched(models_);
	}
	else
	{
		for (auto& model : models_)
		{
			if (model) { model->Draw(); }
		}
	}
	stats_.drawMilliseconds = std::max(
		std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count(),
		0.000001f);

	// DrawSkinnedとCS Dispatchの実行予定数を、可視状態とCompute設定から記録する。
	stats_.drawSkinnedCount = stats_.visibleCount;
	ApplyRuntimeSettings();
}

void AnimationModelBatchTest::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("AnimationModel Batch Test");
	if (ImGui::Checkbox("Enable AnimationModel Batch Test", &enabled_) && enabled_ && models_.empty())
	{
		rebuildRequested_ = true;
	}
	ImGui::Combo("Model Count", &modelCountIndex_, kModelCountLabels, static_cast<int>(std::size(kModelCountLabels)));
	ImGui::DragFloat("Spacing##AnimationModelBatch", &spacing_, 0.1f, 0.5f, 20.0f, "%.2f");
	ImGui::Checkbox("Play Animation", &playAnimation_);
	ImGui::Checkbox("Use Compute Skinning", &useComputeSkinning_);
	ImGui::Checkbox("Use DrawBatched", &useDrawBatched_);
	ImGui::Checkbox("Use Shared Animation Pose", &useSharedAnimationPose_);
	if (ImGui::Button("Force Pose Update"))
	{
		// ボタン要求は次のUpdateで全モデルへ一度だけ適用し、適用後に自動解除する。
		forcePoseUpdateRequest_ = true;
	}
	if (ImGui::Button("Rebuild Test Models"))
	{
		// ImGui中に旧Paletteを破棄せず、次フレームの安全なUpdateまで要求だけを保持する。
		rebuildRequested_ = true;
	}
	ImGui::Text("Rebuild Requested: %s", rebuildRequested_ ? "Yes" : "No");

	ImGui::Text("AnimationModel Count: %zu", stats_.modelCount);
	ImGui::Text("Visible Models: %zu", stats_.visibleCount);
	ImGui::Text("Culled: %zu", stats_.culledCount);
	ImGui::Text("DrawSkinned Calls: %zu", stats_.drawSkinnedCount);
	ImGui::Text("DispatchSkinningCS Calls: %zu", stats_.dispatchSkinningCSCount);
	ImGui::Text("Pose Dirty Count: %zu", stats_.poseDirtyCount);
	ImGui::Text("Palette Update Skipped Count: %zu", stats_.paletteUpdateSkippedCount);
	ImGui::Text("Compute Skinning Skipped Count: %zu", stats_.computeSkinningSkippedCount);
	ImGui::Text("Force Pose Update Request: %s", forcePoseUpdateRequest_ ? "Yes" : "No");
	ImGui::Text("Last Force Pose Update Count: %zu", stats_.lastForcePoseUpdateCount);
	ImGui::Text("Shared Pose Update Count: %zu", stats_.sharedPoseUpdateCount);
	ImGui::Text("Shared Palette Update Count: %zu", stats_.sharedPaletteUpdateCount);
	ImGui::Text("Per Model Palette Update Count: %zu", stats_.perModelPaletteUpdateCount);
	ImGui::Text("Shared Palette Dispatch Count: %zu", stats_.sharedPaletteDispatchCount);
	ImGui::Text("Shared Palette Valid: %s", stats_.sharedPaletteValid ? "true" : "false");
	ImGui::Text("Shared Palette Enabled: %s", useSharedAnimationPose_ && stats_.sharedPaletteValid ? "true" : "false");
	ImGui::Text("Representative-only fallback: %s", stats_.representativeOnlyFallback ? "true" : "false");
	ImGui::Text("Play Animation Time: %.6f s", stats_.playAnimationTimeSeconds);
	ImGui::Text("Animation Duration: %.6f s", stats_.animationDurationSeconds);
	ImGui::Text("Animation Loaded: %s", stats_.animationLoaded ? "true" : "false");
	if (playAnimation_ && !stats_.animationLoaded)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
			"Warning: Animation duration is zero. This model may not contain animation data.");
	}
	ImGui::Text("Total Update Time: %.6f ms", stats_.updateMilliseconds);
	ImGui::Text("WorldTransform Update Time: %.6f ms", stats_.worldTransformMilliseconds);
	ImGui::Text("Animation Time Update: %.6f ms", stats_.animationTimeMilliseconds);
	ImGui::Text("Skeleton Update Time: %.6f ms", stats_.skeletonMilliseconds);
	ImGui::Text("SkinCluster Palette Update Time: %.6f ms", stats_.paletteMilliseconds);
	ImGui::Text("Shared Skeleton Update Time: %.6f ms", stats_.sharedSkeletonMilliseconds);
	ImGui::Text("Shared Palette Update Time: %.6f ms", stats_.sharedPaletteMilliseconds);
	ImGui::Text("PerModel Palette Update Time: %.6f ms", stats_.perModelPaletteMilliseconds);
	ImGui::Text("Palette Share Saved Count: %zu", stats_.paletteShareSavedCount);
	ImGui::Text("Palette Share Saved Time: %.6f ms (estimate)", stats_.paletteShareSavedMilliseconds);
	ImGui::Text("Compute Skinning Time: %.6f ms", stats_.computeSkinningMilliseconds);
	ImGui::Text("Draw Time: %.6f ms", stats_.drawMilliseconds);
	for (size_t lod = 0; lod < stats_.lodCounts.size(); ++lod)
	{
		ImGui::Text("LOD%zu Count: %zu", lod, stats_.lodCounts[lod]);
	}
#endif
}

void AnimationModelBatchTest::RebuildModelsSafely()
{
	// CommandListが参照中のSkinCluster PaletteResourceを即時破棄しないよう、DebugSceneのリビルドはGPU待機後に行う。
	auto* directXCommon = Ken4lowEngine::DirectXCommon::GetInstance();
	if (directXCommon && directXCommon->GetFenceManager() && directXCommon->GetCommandManager())
	{
		directXCommon->GetFenceManager()->Signal(directXCommon->GetCommandManager()->GetCommandQueue());
		directXCommon->GetFenceManager()->Wait();
	}

	for (auto& model : models_)
	{
		if (model) { model->Clear(); }
	}
	models_.clear();
	models_.shrink_to_fit();
	stats_ = {};
	CreateTestModels();
}

void AnimationModelBatchTest::CreateTestModels()
{
	// GPU待機と旧モデル破棄を終えた後にだけ、新しいDebugScene用モデル群を生成する。
	const int modelCount = kModelCounts[std::clamp(modelCountIndex_, 0, static_cast<int>(std::size(kModelCounts)) - 1)];
	const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(modelCount))));
	const int rows = (modelCount + columns - 1) / columns;
	models_.reserve(static_cast<size_t>(modelCount));

	for (int index = 0; index < modelCount; ++index)
	{
		auto model = std::make_unique<Ken4lowEngine::AnimationModel>();
		model->Initialize(kTestModelPath, true);
		model->ReloadAnimationForDebugBatchTest();
		const int x = index % columns;
		const int z = index / columns;
		model->SetTranslate({
			(static_cast<float>(x) - static_cast<float>(columns - 1) * 0.5f) * spacing_,
			0.0f,
			(static_cast<float>(z) - static_cast<float>(rows - 1) * 0.5f) * spacing_
		});
		model->SetComputeSkinningEnabled(useComputeSkinning_);
		model->SetAnimationPlaying(playAnimation_);
		// WVP補正はこのDebugScene専用負荷検証だけに限定し、実ゲームのAnimationModelには適用しない。
		model->SetUseDebugSkinningViewProjection(true);
		if (index == 0)
		{
			stats_.animationDurationSeconds = model->GetAnimationDurationForDebugBatchTest();
			stats_.animationLoaded = model->HasAnimationForDebugBatchTest();
		}
		models_.push_back(std::move(model));
	}
	poseDirty_ = true;
	previousPlayAnimation_ = playAnimation_;
	RefreshVisibilityStats();
}

void AnimationModelBatchTest::RefreshVisibilityStats()
{
	stats_.modelCount = models_.size();
	stats_.visibleCount = 0;
	stats_.culledCount = 0;
	stats_.lodCounts.clear();
	for (const auto& model : models_)
	{
		if (!model) { continue; }
		if (model->IsVisible()) { ++stats_.visibleCount; }
		else { ++stats_.culledCount; }
		const int lod = std::max(0, model->GetLOD());
		if (stats_.lodCounts.size() <= static_cast<size_t>(lod))
		{
			stats_.lodCounts.resize(static_cast<size_t>(lod) + 1, 0);
		}
		++stats_.lodCounts[static_cast<size_t>(lod)];
	}
}

void AnimationModelBatchTest::ApplyRuntimeSettings()
{
	for (auto& model : models_)
	{
		if (!model) { continue; }
		model->SetAnimationPlaying(playAnimation_);
		model->SetComputeSkinningEnabled(useComputeSkinning_);
		// 代表Palette共有中は全Debugモデルを同じLODへ揃え、異なるSkinCluster間の共有を防ぐ。
		model->SetForceLOD(useSharedAnimationPose_, 0);
	}
}
