#pragma once

#include "EditorContext.h"
#include "EditorPanelIds.h"
#include "EditorWindowManager.h"

#include <BaseScene.h>
#include <LightManager.h>
#include <SceneManager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// World OutlinerとDetailsを同じEditorSelectionへ接続し、UE風の階層・検索・Inspector表示を提供します。
	/// </summary>
	class EditorHierarchyPanel
	{
	public:
		static EditorHierarchyPanel* GetInstance()
		{
			static EditorHierarchyPanel instance;
			return &instance;
		}

		void Draw()
		{
#ifdef USE_IMGUI
			CollectCurrentObjects();
			RefreshSelection();
			DrawWorldOutliner();
			DrawDetails();
#endif
		}

	private:
		EditorHierarchyPanel() = default;
		~EditorHierarchyPanel() = default;
		EditorHierarchyPanel(const EditorHierarchyPanel&) = delete;
		EditorHierarchyPanel& operator=(const EditorHierarchyPanel&) = delete;

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

		void CollectCurrentObjects()
		{
			objects_.clear();
			SceneManager* sceneManager = EditorWindowManager::GetInstance()->GetSceneManager();
			BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			if (scene)
			{
				scene->CollectEditorObjects(objects_);
			}

			std::stable_sort(objects_.begin(), objects_.end(), [](const EditorObjectInfo& lhs, const EditorObjectInfo& rhs)
				{
					if (lhs.parentId != rhs.parentId)
					{
						return lhs.parentId < rhs.parentId;
					}
					if (lhs.sortOrder != rhs.sortOrder)
					{
						return lhs.sortOrder < rhs.sortOrder;
					}
					return lhs.displayName < rhs.displayName;
				});
		}

		void RefreshSelection()
		{
			EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
			if (!selection.HasSelection())
			{
				return;
			}

			const EditorObjectInfo& selected = selection.GetSelected();
			const auto current = std::find_if(objects_.begin(), objects_.end(), [&selected](const EditorObjectInfo& object)
				{
					return object.id == selected.id && object.sceneName == selected.sceneName;
				});
			if (current != objects_.end())
			{
				selection.RefreshSelected(*current);
			}
			else
			{
				selection.Clear();
			}
		}

		bool MatchesFilter(const EditorObjectInfo& object) const
		{
			const std::string_view filter(outlinerSearch_.data());
			return ContainsCaseInsensitive(object.displayName, filter) ||
				ContainsCaseInsensitive(object.typeName, filter) ||
				ContainsCaseInsensitive(object.sceneName, filter);
		}

		bool HasMatchingDescendant(uint64_t objectId) const
		{
			for (const EditorObjectInfo& child : objects_)
			{
				if (child.parentId != objectId)
				{
					continue;
				}
				if (MatchesFilter(child) || HasMatchingDescendant(child.id))
				{
					return true;
				}
			}
			return false;
		}

		bool HasChildren(uint64_t objectId) const
		{
			return std::any_of(objects_.begin(), objects_.end(), [objectId](const EditorObjectInfo& object)
				{
					return object.parentId == objectId;
				});
		}

		bool IsSelected(const EditorObjectInfo& object) const
		{
			const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
			return selection.HasSelection() &&
				selection.GetSelected().id == object.id &&
				selection.GetSelected().sceneName == object.sceneName;
		}

		void DrawObjectNode(const EditorObjectInfo& object)
		{
			const std::string_view filter(outlinerSearch_.data());
			const bool filterActive = !filter.empty();
			if (filterActive && !MatchesFilter(object) && !HasMatchingDescendant(object.id))
			{
				return;
			}

			ImGui::PushID(static_cast<int>(object.id & 0x7fffffff));

			bool active = true;
			if (object.ReadActive(active))
			{
				if (ImGui::Checkbox("##OutlinerActive", &active))
				{
					object.WriteActive(active);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip(active ? "有効" : "無効");
				}
				ImGui::SameLine();
			}

			const bool hasChildren = HasChildren(object.id);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick;
			if (!hasChildren)
			{
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (IsSelected(object))
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			if (object.objectKind == EditorObjectKind::Actor)
			{
				flags |= ImGuiTreeNodeFlags_DefaultOpen;
			}
			if (filterActive && hasChildren)
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
			}

			const std::string label = object.icon + "  " + object.displayName + "##OutlinerNode";
			const bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				EditorContext::GetInstance()->GetSelection().Select(object);
			}

			if (ImGui::BeginPopupContextItem("##OutlinerContext"))
			{
				ImGui::TextDisabled("%s", object.typeName.c_str());
				if (object.canRename && ImGui::MenuItem("名前を変更"))
				{
					renameTargetId_ = object.id;
					std::snprintf(renameBuffer_.data(), renameBuffer_.size(), "%s", object.displayName.c_str());
					openRenamePopup_ = true;
				}
				if (object.canDuplicate && ImGui::MenuItem("複製"))
				{
					object.RequestDuplicate();
				}
				if (object.canDelete && ImGui::MenuItem("削除"))
				{
					object.RequestDelete();
				}
				ImGui::EndPopup();
			}

			if (hasChildren && opened)
			{
				for (const EditorObjectInfo& child : objects_)
				{
					if (child.parentId == object.id)
					{
						DrawObjectNode(child);
					}
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		void DrawRenamePopup()
		{
			if (openRenamePopup_)
			{
				ImGui::OpenPopup("名前を変更##OutlinerRename");
				openRenamePopup_ = false;
			}

			if (ImGui::BeginPopupModal("名前を変更##OutlinerRename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::SetNextItemWidth(320.0f);
				ImGui::InputText("新しい名前", renameBuffer_.data(), renameBuffer_.size());
				ImGui::Separator();
				if (ImGui::Button("変更", ImVec2(120.0f, 0.0f)))
				{
					const auto target = std::find_if(objects_.begin(), objects_.end(), [this](const EditorObjectInfo& object)
						{
							return object.id == renameTargetId_;
						});
					if (target != objects_.end())
					{
						target->Rename(renameBuffer_.data());
						EditorContext::GetInstance()->GetSelection().RefreshSelected(*target);
					}
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("キャンセル", ImVec2(120.0f, 0.0f)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		void DrawWorldOutliner()
		{
			auto& windowState = EditorWindowManager::GetInstance()->GetWindowState();
			if (!windowState.showWorldOutliner)
			{
				return;
			}

			const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin(EditorPanelIds::WorldOutliner, &windowState.showWorldOutliner, windowFlags))
			{
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##OutlinerSearch", "アクタやコンポーネントを検索...", outlinerSearch_.data(), outlinerSearch_.size());
				ImGui::TextDisabled("表示中: %zu", objects_.size());
				ImGui::Separator();

				if (ImGui::BeginChild("##WorldOutlinerBody", ImVec2(0.0f, 0.0f), false))
				{
					const char* sceneLabel = objects_.empty() ? "現在のシーン" : objects_.front().sceneName.c_str();
					if (ImGui::TreeNodeEx(sceneLabel, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
					{
						if (objects_.empty())
						{
							ImGui::TextDisabled("表示できるEditorオブジェクトがありません。");
						}
						for (const EditorObjectInfo& object : objects_)
						{
							if (object.parentId == 0)
							{
								DrawObjectNode(object);
							}
						}
						ImGui::TreePop();
					}
				}
				ImGui::EndChild();
				DrawRenamePopup();
			}
			ImGui::End();
		}

		void DrawFallbackTransformInspector(const EditorObjectInfo& selected)
		{
			EditorTransform transform{};
			if (!selected.TryReadTransform(transform))
			{
				ImGui::TextDisabled("%s", selected.transformUnavailableReason.c_str());
				return;
			}

			bool changed = false;
			changed |= ImGui::DragFloat3("位置", &transform.position.x, 0.1f);
			changed |= ImGui::DragFloat3("回転", &transform.rotation.x, 0.01f);
			changed |= ImGui::DragFloat3("スケール", &transform.scale.x, 0.1f, 0.001f, 1000.0f);
			if (changed)
			{
				selected.WriteTransform(transform);
				EditorContext::GetInstance()->MarkLevelDirty();
				// Property変更をDirty状態へ接続し、Phase 10のLevel保存で未保存変更を判定できるようにする。
			}
		}

		void DrawDetails()
		{
			auto& windowState = EditorWindowManager::GetInstance()->GetWindowState();
			if (!windowState.showDetails)
			{
				return;
			}

			const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin(EditorPanelIds::Details, &windowState.showDetails, windowFlags))
			{
				if (ImGui::BeginChild("##DetailsBody", ImVec2(0.0f, 0.0f), false))
				{
					EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
					if (!selection.HasSelection())
					{
						ImGui::TextDisabled("オブジェクトが選択されていません。");
					}
					else
					{
						const EditorObjectInfo& selected = selection.GetSelected();
						ImGui::PushID(static_cast<int>(selected.id & 0x7fffffff)); // 選択オブジェクトごとにDetails全体のID空間を分離する。
						ImGui::Text("%s  %s", selected.icon.c_str(), selected.displayName.c_str());
						ImGui::TextDisabled("%s", selected.typeName.c_str());

						if (selected.canRename)
						{
							std::array<char, 256> nameBuffer{};
							std::snprintf(nameBuffer.data(), nameBuffer.size(), "%s", selected.displayName.c_str());
							ImGui::SetNextItemWidth(-1.0f);
							if (ImGui::InputText("名前##SelectedObjectName", nameBuffer.data(), nameBuffer.size(), ImGuiInputTextFlags_EnterReturnsTrue))
							{
								selected.Rename(nameBuffer.data());
								EditorContext::GetInstance()->MarkLevelDirty();
							}
						}

						bool active = true;
						if (selected.ReadActive(active) && ImGui::Checkbox("有効##SelectedObjectActive", &active))
						{
							selected.WriteActive(active);
							EditorContext::GetInstance()->MarkLevelDirty();
						}

						if (ImGui::CollapsingHeader("基本情報##SelectedObjectInfo", ImGuiTreeNodeFlags_DefaultOpen))
						{
							ImGui::Text("種類: %s", selected.typeName.c_str());
							ImGui::Text("シーン: %s", selected.sceneName.c_str());
							ImGui::TextDisabled("Editor ID: %llu", static_cast<unsigned long long>(selected.id));
							if (!selected.inspectorHint.empty())
							{
								ImGui::TextDisabled("%s", selected.inspectorHint.c_str());
							}
						}

						ImGui::Separator();
						if (selected.drawInspector)
						{
							ImGui::PushID("SelectedObjectInspector");
							selected.drawInspector();
							ImGui::PopID();
						}
						else
						{
							switch (selected.inspectorType)
							{
							case EditorInspectorType::Transform:
								DrawFallbackTransformInspector(selected);
								break;
							case EditorInspectorType::PunctualLights:
								LightManager::GetInstance()->DrawPunctualLightsInspector();
								break;
							default:
								ImGui::TextDisabled("このオブジェクトの詳細表示は未実装です。");
								break;
							}
						}
						ImGui::PopID();
					}
				}
				ImGui::EndChild();
			}
			ImGui::End();
		}
#endif

		std::vector<EditorObjectInfo> objects_;
		std::array<char, 128> outlinerSearch_{};
		std::array<char, 256> renameBuffer_{};
		uint64_t renameTargetId_ = 0;
		bool openRenamePopup_ = false;
	};
} // namespace Ken4lowEngine
