#include "StageMission.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void StageMissionBase::Initialize(GamePlayWorld* world, const GamePlayStageContext& stageContext, const StageMissionConfig& config)
{
	world_ = world;
	stageContext_ = &stageContext;
	config_ = config;
	isCleared_ = false;
	isFailed_ = false;
}

void StageMissionBase::DrawDebugImGui()
{
#ifdef USE_IMGUI
	ImGui::Text("Mission: %s", GetDebugName());
	ImGui::Text("Cleared: %s", isCleared_ ? "true" : "false");
	ImGui::Text("Failed : %s", isFailed_ ? "true" : "false");
#endif
}
