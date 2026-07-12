#pragma once

#include "EditorContext.h"
#include "EditorLevelService.h"
#include "EditorWindowManager.h"

#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
#ifdef USE_IMGUI
	/// <summary>Level保存・読み込み操作をMain Viewport右上へ表示します。</summary>
	inline void DrawEditorLevelOverlay()
	{
		EditorWindowManager* windowManager = EditorWindowManager::GetInstance();
		EditorLevelService* levelService = EditorLevelService::GetInstance();
		levelService->SetSceneManager(windowManager->GetSceneManager());
		levelService->Update(ImGui::GetIO().DeltaTime);
		levelService->UpdateShortcuts();

		const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
		if (viewportRect.valid)
		{
			const float width = 330.0f;
			ImGui::SetNextWindowPos(
				ImVec2(viewportRect.screenMax.x - width - 8.0f, viewportRect.screenMin.y + 8.0f),
				ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(width, 42.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.94f);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			if (ImGui::Begin("##EditorLevelToolbar", nullptr, flags))
			{
				if (ImGui::Button("Level", ImVec2(58.0f, 24.0f))) ImGui::OpenPopup("##EditorLevelMenu");
				if (ImGui::BeginPopup("##EditorLevelMenu"))
				{
					levelService->DrawFileMenuItems();
					ImGui::EndPopup();
				}

				ImGui::SameLine();
				if (ImGui::Button("保存", ImVec2(48.0f, 24.0f))) levelService->RequestSaveLevel();
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Level保存  Ctrl+S");

				ImGui::SameLine();
				const EditorContext* context = EditorContext::GetInstance();
				const std::string displayName = context->GetActiveLevelName() + (context->IsLevelDirty() ? "*" : "");
				ImGui::TextDisabled("%s", displayName.c_str()); // Dirty状態はLevel名末尾の*で常時確認できる。
			}
			ImGui::End();
		}

		levelService->DrawDialogs();

		std::string statusMessage;
		bool succeeded = false;
		if (levelService->ConsumeStatus(statusMessage, succeeded))
		{
			windowManager->AddOutputLog(succeeded ? EditorLogLevel::Info : EditorLogLevel::Error, statusMessage);
		}
	}
#endif
} // namespace Ken4lowEngine
