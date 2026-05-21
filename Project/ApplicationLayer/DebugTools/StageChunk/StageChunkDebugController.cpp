#include "StageChunkDebugController.h"

#include "Stage.h"
#include "CameraManager.h"

#include <cstddef>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StageChunkDebugController::DrawImGui(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	// 単体表示時もDocking可能な通常ウィンドウとして開く。
	if (ImGui::Begin("StageChunk Culling Debug"))
	{
		DrawImGuiContent(stage);
	}
	ImGui::End();
#else
	(void)stage;
#endif
}

void StageChunkDebugController::DrawImGuiContent(K4E::Stage* stage)
{
#ifdef USE_IMGUI
	if (!stage) { return; }

	auto& chunkManager = stage->GetStageChunkManager();
	auto stats = chunkManager.GetStatistics();
	bool enabled = stage->IsStageChunkCullingEnabled();
	bool showBounds = stage->IsStageChunkBoundsVisible();
	bool showObjectBounds = stage->IsStageChunkObjectBoundsVisible();
	bool autoExcludeLargeObjects = stage->IsStageChunkAutoExcludeLargeObjects();
	float chunkSize = stage->GetStageChunkSize();
	int selectedMeshIndex = static_cast<int>(chunkManager.GetDebugSelectedMeshIndex());

	if (ImGui::Checkbox("StageChunk Culling 有効", &enabled))
	{
		stage->SetStageChunkCullingEnabled(enabled);
	}
	if (ImGui::DragFloat("Chunkサイズ", &chunkSize, 1.0f, 1.0f, 200.0f))
	{
		stage->SetStageChunkSize(chunkSize);
	}
	if (ImGui::Checkbox("巨大ObjectをChunk Culling対象外", &autoExcludeLargeObjects))
	{
		stage->SetStageChunkAutoExcludeLargeObjects(autoExcludeLargeObjects);
	}
	if (ImGui::Checkbox("Chunk Bounds表示", &showBounds))
	{
		stage->SetStageChunkBoundsVisible(showBounds);
	}
	if (ImGui::Checkbox("Object Bounds表示", &showObjectBounds))
	{
		stage->SetStageChunkObjectBoundsVisible(showObjectBounds);
	}
	if (ImGui::InputInt("選択中Object(Mesh) Index", &selectedMeshIndex))
	{
		if (selectedMeshIndex < 0)
		{
			selectedMeshIndex = 0;
		}
		chunkManager.SetDebugSelectedMeshIndex(static_cast<size_t>(selectedMeshIndex));
		stage->RebuildStageChunks();
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
	ImGui::Text("選択中Objectの所属Chunk数: %d", stats.selectedObjectChunkCount);
	ImGui::Text("Chunkに登録されたObject数: %d", stats.totalObjectCountInChunks);
	ImGui::Text("Chunkに登録されたMesh数: %d", stats.totalMeshCountInChunks);
	ImGui::Text("Bounds未設定で描画したObject数: %d", stats.boundsUnsetDrawObjectCount);
	ImGui::Text("Chunk外扱いで描画したObject数: %d", stats.chunkOutsideDrawObjectCount);
	ImGui::Text("Chunk Culling対象外Object数: %d", stats.chunkCullingIgnoredObjectCount);
	ImGui::Text("大きすぎてChunk Culling除外されたObject数: %d", stats.largeObjectExcludedCount);
	if (const auto* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera())
	{
		ImGui::Text("MainCamera Near/Far: %.3f / %.1f", mainCamera->GetNearClip(), mainCamera->GetFarClip());
	}
	ImGui::TextWrapped("可視Chunkは緑、カリングChunkは赤、Object Boundsは青、Chunk Culling対象外Boundsは黄で表示します。");
	ImGui::TextWrapped("床や壁など大きいBoundsは複数Chunk登録または安全側でChunk Culling対象外にし、見えているObjectを消さないようにDraw側だけを制御します。");
#else
	(void)stage;
#endif
}
