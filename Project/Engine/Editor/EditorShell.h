#pragma once

#include "EditorContext.h"
#include "EditorModeController.h"
#include "EditorPanelIds.h"
#include "EditorPlayController.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <Wireframe.h>

#include <algorithm>
#include <array>
#include <cctype>
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
				return; // ゲームプレビュー中はEditor専用パネルを表示しない。
			}

			ApplyViewportVisualPolicy();
			DrawPlaceActors();
#endif
		}

		/// <summary>
		/// メインビューポートの描画後に、操作モードと表示モードを切り替えるツールバーを重ねます。
		/// </summary>
		void DrawViewportOverlay()
		{
#ifdef USE_IMGUI
			if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
			{
				return;
			}

			ApplyViewportVisualPolicy();
			DrawViewportToolbar();
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
			if (filter.empty())
			{
				return true;
			}

			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char c) { return ToLowerAscii(c); });
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		static bool MatchesPlaceableFilter(const PlaceableEntry& entry, std::string_view filter)
		{
			// 日本語名と英語キーワードの両方を検索対象にして、従来の入力にも対応する。
			return ContainsCaseInsensitive(entry.label, filter) || ContainsCaseInsensitive(entry.searchKeywords, filter);
		}

		void ApplyViewportVisualPolicy()
		{
			const auto* viewportController = EditorViewportController::GetInstance();
			const bool drawEditorVisuals = EditorModeController::GetInstance()->ShouldDrawDebugVisuals() &&
				viewportController->IsEditorDisplay() && viewportController->IsAuxiliaryDisplayEnabled();
			// ゲーム表示ではワイヤー、コライダー、ライト範囲などのEditor補助描画をまとめて除外する。
			Wireframe::GetInstance()->SetDebugDrawEnabled(drawEditorVisuals);
		}

		void DrawViewportToolButton(const char* label, EditorViewportTool tool)
		{
			auto* viewportController = EditorViewportController::GetInstance();
			const bool selected = viewportController->GetTool() == tool;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
			}

			if (ImGui::Button(label, ImVec2(54.0f, 0.0f)))
			{
				viewportController->SetTool(tool);
			}

			if (selected)
			{
				ImGui::PopStyleColor();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("現在の操作モード: %s\n変形ギズモへの接続はPhase 8で行います。", label);
			}
		}

		void DrawViewportToolbar()
		{
			auto* windowManager = EditorWindowManager::GetInstance();
			const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
			if (!viewportRect.valid)
			{
				return;
			}

			const float viewportWidth = viewportRect.screenMax.x - viewportRect.screenMin.x;
			if (viewportWidth < 430.0f)
			{
				return;
			}

			ImGui::SetNextWindowPos(ImVec2(viewportRect.screenMin.x + 8.0f, viewportRect.screenMin.y + 8.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(viewportWidth - 16.0f, 42.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.92f);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			if (ImGui::Begin(EditorPanelIds::ViewportToolbarOverlay, nullptr, flags))
			{
				DrawViewportToolButton("選択", EditorViewportTool::Select);
				ImGui::SameLine();
				DrawViewportToolButton("移動", EditorViewportTool::Translate);
				ImGui::SameLine();
				DrawViewportToolButton("回転", EditorViewportTool::Rotate);
				ImGui::SameLine();
				DrawViewportToolButton("拡縮", EditorViewportTool::Scale);

				ImGui::SameLine();
				ImGui::TextDisabled("|");
				ImGui::SameLine();

				auto* viewportController = EditorViewportController::GetInstance();
				ImGui::SetNextItemWidth(130.0f);
				if (ImGui::BeginCombo("##ビューポート表示", viewportController->GetDisplayModeText()))
				{
					const bool editorSelected = viewportController->IsEditorDisplay();
					if (ImGui::Selectable("エディター表示", editorSelected))
					{
						viewportController->SetDisplayMode(EditorViewportDisplayMode::Editor);
					}
					const bool gameSelected = viewportController->IsGameDisplay();
					if (ImGui::Selectable("ゲーム表示", gameSelected))
					{
						viewportController->SetDisplayMode(EditorViewportDisplayMode::Game);
					}
					ImGui::EndCombo();
				}

				ImGui::SameLine();
				bool auxiliaryDisplayEnabled = viewportController->IsAuxiliaryDisplayEnabled();
				if (viewportController->IsGameDisplay())
				{
					ImGui::BeginDisabled();
				}
				if (ImGui::Checkbox("補助表示", &auxiliaryDisplayEnabled))
				{
					viewportController->SetAuxiliaryDisplayEnabled(auxiliaryDisplayEnabled);
				}
				if (viewportController->IsGameDisplay())
				{
					ImGui::EndDisabled();
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("グリッド、コライダー、ライト範囲などのEditor補助描画を切り替えます。");
				}

				ImGui::SameLine();
				bool performanceVisible = windowManager->IsPerformanceOverlayVisible();
				if (ImGui::Checkbox("統計", &performanceVisible))
				{
					windowManager->SetPerformanceOverlayVisible(performanceVisible);
				}

				if (viewportWidth >= 760.0f)
				{
					auto* playController = EditorPlayController::GetInstance();
					ImGui::SameLine();
					ImGui::TextDisabled("状態: %s", playController->GetPlayStateText());
					if (playController->IsPlaying())
					{
						ImGui::SameLine();
						if (ImGui::Button(playController->IsGameCaptured() ? "入力を解放" : "ゲーム入力を取得"))
						{
							playController->ToggleInputCapture();
							// 再生状態は維持したまま、Main Viewportへ渡す入力だけを切り替える。
						}
					}
				}
			}
			ImGui::End();
		}

		void DrawPlaceableCategory(const char* categoryName, const PlaceableEntry* entries, std::size_t count)
		{
			// ImGuiの検索入力バッファをnull終端文字列としてstring_viewへ変換する。
			const std::string_view searchFilter{ placeActorsSearch_.data() };

			bool hasVisibleEntry = false;
			for (std::size_t index = 0; index < count; ++index)
			{
				hasVisibleEntry |= MatchesPlaceableFilter(entries[index], searchFilter);
			}

			if (!hasVisibleEntry)
			{
				return;
			}

			if (!ImGui::CollapsingHeader(categoryName, ImGuiTreeNodeFlags_DefaultOpen))
			{
				return;
			}

			for (std::size_t index = 0; index < count; ++index)
			{
				const PlaceableEntry& entry = entries[index];
				if (!MatchesPlaceableFilter(entry, searchFilter))
				{
					continue;
				}

				ImGui::PushID(entry.searchKeywords);
				if (ImGui::Button(entry.label, ImVec2(-1.0f, 34.0f)))
				{
					EditorContext::GetInstance()->QueuePlacement(entry.type, entry.label);
					// Phase 1では配置要求だけを保存し、実際の生成は後続Phaseで接続する。
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", entry.description);
				}
				ImGui::PopID();
			}
		}

		void DrawPlaceActors()
		{
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
	};
} // namespace Ken4lowEngine
