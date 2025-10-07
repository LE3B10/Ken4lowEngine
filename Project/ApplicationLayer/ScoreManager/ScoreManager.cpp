#include "ScoreManager.h"
#include <imgui.h>

void ScoreManager::Initialize()
{
	Reset();
}

void ScoreManager::AddKill()
{
	++kills_;
	AddScore(100); // 1体倒すごとに100点
}

void ScoreManager::Reset()
{
	score_ = 0;
	kills_ = 0;
}

void ScoreManager::DrawImGui()
{
	ImGui::Begin("Score");
	ImGui::Text("Score: %d", score_);
	ImGui::Text("Kills: %d", kills_);
	ImGui::End();
}
