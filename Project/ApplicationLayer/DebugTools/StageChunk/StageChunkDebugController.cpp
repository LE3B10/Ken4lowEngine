#include "StageChunkDebugController.h"

#include "Stage.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StageChunkDebugController::DrawImGui(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	if (!stage) { return; }

	auto& chunkManager = stage->GetStageChunkManager();
	auto stats = chunkManager.GetStatistics();
	bool enabled = stage->IsStageChunkCullingEnabled();
	bool showBounds = stage->IsStageChunkBoundsVisible();
	float chunkSize = stage->GetStageChunkSize();

	ImGui::Begin("StageChunk Culling Debug");
	if (ImGui::Checkbox("StageChunk Culling 有効", &enabled))
	{
		stage->SetStageChunkCullingEnabled(enabled);
	}
	if (ImGui::DragFloat("Chunkサイズ", &chunkSize, 1.0f, 1.0f, 200.0f))
	{
		stage->SetStageChunkSize(chunkSize);
	}
	if (ImGui::Checkbox("Chunk Bounds表示", &showBounds))
	{
		stage->SetStageChunkBoundsVisible(showBounds);
	}
	if (ImGui::Button("Chunk再構築"))
	{
		stage->RebuildStageChunks();
	}

	stats = chunkManager.GetStatistics();
	ImGui::Separator();
	ImGui::Text("総Chunk数: %d", stats.totalChunkCount);
	ImGui::Text("描画Chunk数: %d", stats.drawnChunkCount);
	ImGui::Text("カリングChunk数: %d", stats.culledChunkCount);
	ImGui::Text("Chunk内Object数: %d", stats.totalObjectCountInChunks);
	ImGui::Text("Chunk内Mesh数: %d", stats.totalMeshCountInChunks);
	ImGui::TextWrapped("DebugCameraで外側から見て、カリングカメラをMainCameraにするとMainCameraのFrustum内Chunkだけが描画されます。");
	ImGui::End();
#else
	(void)stage;
#endif
}
