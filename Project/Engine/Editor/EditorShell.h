#pragma once

#include "EditorContext.h"
#include "EditorModeController.h"
#include "EditorPanelIds.h"

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
				return; // Game Preview中はEditor専用Panelを表示しない。
			}
			DrawPlaceActors();
#endif
		}

	private:
		struct PlaceableEntry
		{
			const char* label;
			const char* description;
			EditorPlaceableType type;
		};

		EditorShell() = default;
		~EditorShell() = default;
		EditorShell(const EditorShell&) = delete;
		EditorShell& operator=(const EditorShell&) = delete;

#ifdef USE_IMGUI
		static bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty())
			{
				return true;
			}

			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		void DrawPlaceableCategory(const char* categoryName, const PlaceableEntry* entries, std::size_t count)
		{
			// ImGuiの固定長入力バッファを終端文字列としてstring_viewへ明示変換する。
			const std::string_view searchFilter(placeActorsSearch_.data());

			bool hasVisibleEntry = false;
			for (std::size_t index = 0; index < count; ++index)
			{
				hasVisibleEntry |= ContainsCaseInsensitive(entries[index].label, searchFilter);
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
				if (!ContainsCaseInsensitive(entry.label, searchFilter))
				{
					continue;
				}

				ImGui::PushID(static_cast<int>(index));
				if (ImGui::Button(entry.label, ImVec2(-1.0f, 34.0f)))
				{
					EditorContext::GetInstance()->QueuePlacement(entry.type, entry.label);
					// Phase 1では配置対象を選び、実際のViewport生成はPhase 6のDrag＆Dropへ接続する。
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
				PlaceableEntry{ "Empty Actor", "Create an Actor with only a root SceneComponent.", EditorPlaceableType::EmptyActor },
				PlaceableEntry{ "Cube", "Create a cube ModelComponent Actor.", EditorPlaceableType::Cube },
				PlaceableEntry{ "Sphere", "Create a sphere ModelComponent Actor.", EditorPlaceableType::Sphere },
				PlaceableEntry{ "Plane", "Create a plane ModelComponent Actor.", EditorPlaceableType::Plane },
			};
			constexpr std::array<PlaceableEntry, 3> lightEntries = {
				PlaceableEntry{ "Directional Light", "Create a directional LightComponent Actor.", EditorPlaceableType::DirectionalLight },
				PlaceableEntry{ "Point Light", "Create a point LightComponent Actor.", EditorPlaceableType::PointLight },
				PlaceableEntry{ "Spot Light", "Create a spot LightComponent Actor.", EditorPlaceableType::SpotLight },
			};
			constexpr std::array<PlaceableEntry, 2> volumeEntries = {
				PlaceableEntry{ "Trigger Box", "Create an overlap-only box trigger Actor.", EditorPlaceableType::TriggerBox },
				PlaceableEntry{ "Trigger Sphere", "Create an overlap-only sphere trigger Actor.", EditorPlaceableType::TriggerSphere },
			};

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
			if (ImGui::Begin(EditorPanelIds::PlaceActors, nullptr, flags))
			{
				ImGui::TextUnformatted("Place Actors");
				ImGui::TextDisabled("Select an item now; viewport placement is connected in Phase 6.");
				ImGui::Separator();
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##PlaceActorsSearch", "Search classes...", placeActorsSearch_.data(), placeActorsSearch_.size());
				ImGui::Spacing();

				DrawPlaceableCategory("Basic", basicEntries.data(), basicEntries.size());
				DrawPlaceableCategory("Lights", lightEntries.data(), lightEntries.size());
				DrawPlaceableCategory("Volumes", volumeEntries.data(), volumeEntries.size());

				const EditorPlacementRequest& request = EditorContext::GetInstance()->GetPlacementRequest();
				if (request.pending)
				{
					ImGui::Separator();
					ImGui::Text("Selected: %s", request.displayName.c_str());
					if (ImGui::Button("Cancel Placement", ImVec2(-1.0f, 0.0f)))
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
