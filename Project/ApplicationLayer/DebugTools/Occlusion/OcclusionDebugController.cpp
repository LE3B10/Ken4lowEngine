#include "OcclusionDebugController.h"

#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void OcclusionDebugController::DrawImGui(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	if (!stage) { return; }

	auto& occlusionSystem = stage->GetOcclusionCullingSystem();
	bool enabled = occlusionSystem.IsEnabled();
	bool showOccluderBounds = occlusionSystem.IsShowOccluderBounds();
	bool showOccludedBounds = occlusionSystem.IsShowOccludedBounds();
	float coverageThreshold = occlusionSystem.GetCoverageThreshold();
	float depthBias = occlusionSystem.GetDepthBias();
	float occlusionMargin = occlusionSystem.GetOcclusionMargin();

	ImGui::Begin("Occlusion Culling Debug");
	if (ImGui::Checkbox("Occlusion Culling 有効", &enabled))
	{
		occlusionSystem.SetEnabled(enabled);
	}
	if (ImGui::Checkbox("Occluder Bounds表示", &showOccluderBounds))
	{
		occlusionSystem.SetShowOccluderBounds(showOccluderBounds);
	}
	if (ImGui::Checkbox("Occluded Bounds表示", &showOccludedBounds))
	{
		occlusionSystem.SetShowOccludedBounds(showOccludedBounds);
	}

	if (ImGui::SliderFloat("coverageThreshold", &coverageThreshold, 0.0f, 1.0f))
	{
		occlusionSystem.SetCoverageThreshold(coverageThreshold);
	}
	if (ImGui::DragFloat("depthBias", &depthBias, 0.001f, 0.0f, 1.0f))
	{
		occlusionSystem.SetDepthBias(depthBias);
	}
	if (ImGui::DragFloat("occlusionMargin", &occlusionMargin, 0.001f, 0.0f, 0.25f))
	{
		occlusionSystem.SetOcclusionMargin(occlusionMargin);
	}

	const auto& stats = occlusionSystem.GetStatistics();
	ImGui::Separator();
	ImGui::Text("Occluder数: %d", stats.occluderCount);
	ImGui::Text("Occlusion判定対象Chunk数: %d", stats.testedChunkCount);
	ImGui::Text("OcclusionでカリングされたChunk数: %d", stats.occludedChunkCount);
	ImGui::TextWrapped("StageChunk Culling 後の Chunk Bounds をスクリーン矩形に投影し、Occluder に十分覆われ、かつ奥にある場合だけ Draw をスキップします。");
	ImGui::TextWrapped("Frustum Culling のカリング Chunk は赤、Occlusion Culling のカリング Chunk は紫、Occluder は黄で表示します。");
	ImGui::End();
#else
	(void)stage;
#endif
}
