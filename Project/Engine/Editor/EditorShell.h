#pragma once

#include "EditorAssetPlacementService.h"
#include "EditorContentBrowserPanel.h"
#include "EditorContext.h"
#include "EditorHierarchyPanel.h"
#include "EditorModeController.h"
#include "EditorPanelIds.h"
#include "EditorPlayController.h"
#include "EditorTransformGizmo.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <CameraManager.h>
#include <DebugCamera.h>
#include <Input.h>
#include <Wireframe.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// DockSpaceへ常設するEditor Shell固有パネルを描画します。
	/// </summary>
	class EditorShell
	{
	public:
		static EditorShell* GetInstance()
		{
			static EditorShell instance;
			return &instance;
		}

		void Draw()
		{
#ifdef USE_IMGUI
			if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
			{
				return;
			}

			ApplyViewportVisualPolicy();
			DrawPlaceActors();
			EditorContentBrowserPanel::GetInstance()->Draw();
			EditorContentBrowserPanel::GetInstance()->UpdateAssetDragSource();
			EditorHierarchyPanel::GetInstance()->Draw();
#endif
		}

		void DrawViewportOverlay()
		{
#ifdef USE_IMGUI
			if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
			{
				return;
			}

			ApplyViewportVisualPolicy();
			DrawViewportToolbar();
			EditorTransformGizmo::GetInstance()->Draw();
			EditorContentBrowserPanel::GetInstance()->UpdateViewportPicking();
			DrawViewportAssetDropTarget();
			UpdateEditorCameraNavigation();
#endif
		}

	private:
		struct PlaceableEntry
		{
			const char* label;
			const char* searchKeywords;
			const char* description;
			EditorPlaceableType type;
		};

#ifdef USE_IMGUI
		enum class ToolbarGlyph
		{
			Save,
			Play,
			Pause,
			Stop,
			Build,
		};
#endif

		EditorShell() = default;
		~EditorShell() = default;
		EditorShell(const EditorShell&) = delete;
		EditorShell& operator=(const EditorShell&) = delete;

#ifdef USE_IMGUI
		static char ToLowerAscii(unsigned char character)
		{
			if (character >= 'A' && character <= 'Z')
			{
				return static_cast<char>(character - 'A' + 'a');
			}
			return static_cast<char>(character);
		}

		static bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty()) return true;
			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		static bool MatchesPlaceableFilter(const PlaceableEntry& entry, std::string_view filter)
		{
			return ContainsCaseInsensitive(entry.label, filter) || ContainsCaseInsensitive(entry.searchKeywords, filter);
		}

		void ApplyViewportVisualPolicy()
		{
			const auto* viewportController = EditorViewportController::GetInstance();
			const bool drawEditorVisuals = EditorModeController::GetInstance()->ShouldDrawDebugVisuals() &&
				viewportController->IsEditorDisplay() && viewportController->IsAuxiliaryDisplayEnabled();
			Wireframe::GetInstance()->SetDebugDrawEnabled(drawEditorVisuals);
		}

		void ReleaseEditorCameraCursor()
		{
			if (!cameraLookActive_) return;
			cameraLookActive_ = false;
			Input::GetInstance()->SetLockCursor(false);
			Input::GetInstance()->SetCursorVisible(true);
		}

		void UpdateEditorCameraNavigation()
		{
			EditorPlayController* playController = EditorPlayController::GetInstance();
			EditorViewportController* viewportController = EditorViewportController::GetInstance();
			CameraManager* cameraManager = CameraManager::GetInstance();
			const bool useEditorCamera = !playController->IsPlaying() && viewportController->IsEditorDisplay();
			cameraManager->SetUseDebugCamera(useEditorCamera);
			if (!useEditorCamera)
			{
				ReleaseEditorCameraCursor();
				return;
			}

			const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
			if (!viewportRect.valid || EditorTransformGizmo::GetInstance()->IsUsing() || ImGui::GetDragDropPayload())
			{
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) ReleaseEditorCameraCursor();
				return;
			}

			if (viewportRect.isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				cameraLookActive_ = true;
				Input::GetInstance()->SetGameInputEnabled(false);
				Input::GetInstance()->SetLockCursor(true);
				Input::GetInstance()->SetCursorVisible(false); // RMB開始フレームからRaw相対入力モードへ切り替える。
			}
			if (cameraLookActive_ && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				ReleaseEditorCameraCursor();
			}

			const bool middlePan = viewportRect.isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
			const bool wheelDolly = viewportRect.isHovered && std::abs(ImGui::GetIO().MouseWheel) > 0.001f;
			if (!cameraLookActive_ && !middlePan && !wheelDolly) return;

			Input* input = Input::GetInstance();
			const ImGuiIO& io = ImGui::GetIO();
			Vector3 localMove{};
			float pitchDelta = 0.0f;
			float yawDelta = 0.0f;

			if (cameraLookActive_)
			{
				input->SetGameInputEnabled(false);
				input->SetLockCursor(true);
				input->SetCursorVisible(false);
				const float deltaTime = std::clamp(io.DeltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
				const float moveSpeed = input->PushRawKey(DIK_LSHIFT) ? 24.0f : 8.0f;
				const float moveStep = moveSpeed * deltaTime;
				if (input->PushRawKey(DIK_W)) localMove.z += moveStep;
				if (input->PushRawKey(DIK_S)) localMove.z -= moveStep;
				if (input->PushRawKey(DIK_A)) localMove.x -= moveStep;
				if (input->PushRawKey(DIK_D)) localMove.x += moveStep;
				if (input->PushRawKey(DIK_Q)) localMove.y -= moveStep;
				if (input->PushRawKey(DIK_E)) localMove.y += moveStep;
			}

			if (middlePan)
			{
				localMove.x -= io.MouseDelta.x * 0.015f;
				localMove.y += io.MouseDelta.y * 0.015f;
			}
			if (wheelDolly) localMove.z += io.MouseWheel * 2.0f;

			DebugCamera* debugCamera = cameraManager->GetDebugCamera();
			if (debugCamera)
			{
				debugCamera->ApplyEditorNavigation(localMove, pitchDelta, yawDelta);
			}
		}

		bool DrawToolbarGlyphButton(const char* id, ToolbarGlyph glyph, bool enabled, const char* tooltip)
		{
			if (!enabled) ImGui::BeginDisabled();
			const ImVec2 buttonSize{ 28.0f, 24.0f };
			const bool clicked = ImGui::InvisibleButton(id, buttonSize);
			const ImVec2 minimum = ImGui::GetItemRectMin();
			const ImVec2 maximum = ImGui::GetItemRectMax();
			const ImVec2 center{ (minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f };
			const ImU32 color = ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			switch (glyph)
			{
			case ToolbarGlyph::Save:
				drawList->AddRect(ImVec2(center.x - 7.0f, center.y - 8.0f), ImVec2(center.x + 7.0f, center.y + 8.0f), color, 1.0f, 0, 1.8f);
				drawList->AddRectFilled(ImVec2(center.x - 4.5f, center.y - 7.0f), ImVec2(center.x + 3.0f, center.y - 2.0f), color, 0.5f);
				drawList->AddRect(ImVec2(center.x - 4.5f, center.y + 1.0f), ImVec2(center.x + 4.5f, center.y + 6.5f), color, 0.5f, 0, 1.4f);
				break;
			case ToolbarGlyph::Play:
				drawList->AddTriangleFilled(ImVec2(center.x - 5.0f, center.y - 7.0f), ImVec2(center.x - 5.0f, center.y + 7.0f), ImVec2(center.x + 7.0f, center.y), color);
				break;
			case ToolbarGlyph::Pause:
				drawList->AddRectFilled(ImVec2(center.x - 6.0f, center.y - 7.0f), ImVec2(center.x - 2.0f, center.y + 7.0f), color, 0.8f);
				drawList->AddRectFilled(ImVec2(center.x + 2.0f, center.y - 7.0f), ImVec2(center.x + 6.0f, center.y + 7.0f), color, 0.8f);
				break;
			case ToolbarGlyph::Stop:
				drawList->AddRectFilled(ImVec2(center.x - 6.5f, center.y - 6.5f), ImVec2(center.x + 6.5f, center.y + 6.5f), color, 1.0f);
				break;
			case ToolbarGlyph::Build:
				drawList->AddLine(ImVec2(center.x - 6.0f, center.y + 7.0f), ImVec2(center.x + 4.0f, center.y - 3.0f), color, 3.0f);
				drawList->AddRectFilled(ImVec2(center.x + 1.0f, center.y - 7.0f), ImVec2(center.x + 8.0f, center.y - 1.0f), color, 1.0f);
				break;
			}

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tooltip);
			if (!enabled) ImGui::EndDisabled();
			return clicked && enabled;
		}

		void DrawPlaybackControls()
		{
			EditorPlayController* playController = EditorPlayController::GetInstance();
			EditorWindowManager* windowManager = EditorWindowManager::GetInstance();

			if (DrawToolbarGlyphButton("##SaveLevelIcon", ToolbarGlyph::Save, true, "レベル保存（Phase 10で接続）"))
			{
				windowManager->AddOutputLog(EditorLogLevel::Info, "レベル保存はPhase 10で接続します。");
			}
			ImGui::SameLine();

			const bool canStartOrResume = playController->IsEditing() || playController->IsPaused();
			if (DrawToolbarGlyphButton("##PlayIcon", ToolbarGlyph::Play, canStartOrResume, playController->IsPaused() ? "再開" : "再生"))
			{
				playController->Play();
			}
			ImGui::SameLine();

			if (DrawToolbarGlyphButton("##PauseIcon", ToolbarGlyph::Pause, playController->IsPlaying(), "一時停止"))
			{
				playController->Pause();
			}
			ImGui::SameLine();

			if (DrawToolbarGlyphButton("##StopIcon", ToolbarGlyph::Stop, !playController->IsEditing(), "停止して編集状態へ戻る"))
			{
				playController->Stop();
			}
			ImGui::SameLine();

			const bool buildEnabled = !windowManager->IsAssetBuildRunning();
			if (DrawToolbarGlyphButton("##BuildIcon", ToolbarGlyph::Build, buildEnabled, buildEnabled ? "全アセットをビルド" : "アセットをビルド中"))
			{
				windowManager->StartAllAssetBuild();
			}
		}

		void DrawViewportToolbar()
		{
			const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
			if (!viewportRect.valid) return;

			const ImVec2 toolbarPosition(viewportRect.screenMin.x + 8.0f, viewportRect.screenMin.y + 8.0f);
			const ImVec2 toolbarSize(std::max(440.0f, viewportRect.imageSize.x - 16.0f), 40.0f);
			ImGui::SetNextWindowPos(toolbarPosition, ImGuiCond_Always);
			ImGui::SetNextWindowSize(toolbarSize, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.88f);
			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin("##MainViewportToolbar", nullptr, flags))
			{
				DrawPlaybackControls();
				ImGui::SameLine();
				ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
				ImGui::SameLine();
				EditorViewportController* controller = EditorViewportController::GetInstance();
				const auto toolButton = [controller](const char* label, EditorViewportTool tool)
					{
						const bool selected = controller->GetTool() == tool;
						if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
						if (ImGui::Button(label)) controller->SetTool(tool);
						if (selected) ImGui::PopStyleColor();
					};
				toolButton("選択", EditorViewportTool::Select); ImGui::SameLine();
				toolButton("移動", EditorViewportTool::Translate); ImGui::SameLine();
				toolButton("回転", EditorViewportTool::Rotate); ImGui::SameLine();
				toolButton("拡縮", EditorViewportTool::Scale); ImGui::SameLine();

				if (ImGui::Button(controller->GetGizmoSpaceText())) controller->ToggleGizmoSpace();
				ImGui::SameLine();
				bool snap = controller->IsSnapEnabled();
				if (ImGui::Checkbox("Snap", &snap)) controller->SetSnapEnabled(snap);
				ImGui::SameLine();
				if (ImGui::BeginCombo("##ViewportDisplay", controller->GetDisplayModeText()))
				{
					if (ImGui::Selectable("エディター表示", controller->IsEditorDisplay())) controller->SetDisplayMode(EditorViewportDisplayMode::Editor);
					if (ImGui::Selectable("ゲーム表示", controller->IsGameDisplay())) controller->SetDisplayMode(EditorViewportDisplayMode::Game);
					ImGui::EndCombo();
				}
			}
			ImGui::End();
		}

		void DrawViewportAssetDropTarget()
		{
			EditorAssetPlacementService::GetInstance()->DrawViewportDropTarget();
		}

		void DrawPlaceableCategory(const char* categoryName, const PlaceableEntry* entries, size_t count)
		{
			if (!ImGui::CollapsingHeader(categoryName, ImGuiTreeNodeFlags_DefaultOpen)) return;
			const std::string_view filter(placeActorsSearch_.data());
			for (size_t i = 0; i < count; ++i)
			{
				const PlaceableEntry& entry = entries[i];
				if (!MatchesPlaceableFilter(entry, filter)) continue;
				if (ImGui::Selectable(entry.label))
				{
					EditorContext::GetInstance()->QueuePlacement(entry.type, entry.label);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.description);
			}
		}

		void DrawPlaceActors()
		{
			const auto& windowState = EditorWindowManager::GetInstance()->GetWindowState();
			if (!windowState.showScene) return;

			constexpr std::array<PlaceableEntry, 4> basicEntries = {
				PlaceableEntry{ "空のアクタ", "Empty Actor", "ルートSceneComponentだけを持つアクタを作成します。", EditorPlaceableType::EmptyActor },
				PlaceableEntry{ "キューブ", "Cube", "キューブモデルを持つアクタを作成します。", EditorPlaceableType::Cube },
				PlaceableEntry{ "スフィア", "Sphere", "球体モデルを持つアクタを作成します。", EditorPlaceableType::Sphere },
				PlaceableEntry{ "平面", "Plane", "平面モデルを持つアクタを作成します。", EditorPlaceableType::Plane },
			};
			constexpr std::array<PlaceableEntry, 3> lightEntries = {
				PlaceableEntry{ "ディレクショナルライト", "Directional Light", "平行光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::DirectionalLight },
				PlaceableEntry{ "ポイントライト", "Point Light", "点光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::PointLight },
				PlaceableEntry{ "スポットライト", "Spot Light", "スポット光源のLightComponentを持つアクタを作成します。", EditorPlaceableType::SpotLight },
			};
			constexpr std::array<PlaceableEntry, 2> volumeEntries = {
				PlaceableEntry{ "トリガーボックス", "Trigger Box", "Overlap専用のBox Triggerアクタを作成します。", EditorPlaceableType::TriggerBox },
				PlaceableEntry{ "トリガースフィア", "Trigger Sphere", "Overlap専用のSphere Triggerアクタを作成します。", EditorPlaceableType::TriggerSphere },
			};

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
			if (ImGui::Begin(EditorPanelIds::PlaceActors, nullptr, flags))
			{
				ImGui::TextUnformatted("アクタを配置");
				ImGui::TextDisabled("配置する種類を選択してください。実際の配置は後続Phaseで接続します。");
				ImGui::Separator();
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##PlaceActorsSearch", "クラスを検索...", placeActorsSearch_.data(), placeActorsSearch_.size());
				ImGui::Spacing();
				DrawPlaceableCategory("基本", basicEntries.data(), basicEntries.size());
				DrawPlaceableCategory("ライト", lightEntries.data(), lightEntries.size());
				DrawPlaceableCategory("ボリューム", volumeEntries.data(), volumeEntries.size());

				const EditorPlacementRequest& request = EditorContext::GetInstance()->GetPlacementRequest();
				if (request.pending)
				{
					ImGui::Separator();
					ImGui::Text("選択中: %s", request.displayName.c_str());
					if (ImGui::Button("配置をキャンセル", ImVec2(-1.0f, 0.0f)))
					{
						EditorContext::GetInstance()->ClearPlacementRequest();
					}
				}
			}
			ImGui::End();
		}
#endif

		std::array<char, 96> placeActorsSearch_{};
		bool cameraLookActive_ = false;
	};
} // namespace Ken4lowEngine
