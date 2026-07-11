#pragma once

#include "EditorAssetRegistryV2.h"
#include "EditorWindowManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// UE風のフォルダツリー・パンくず・アセットグリッドを持つコンテンツブラウザです。
	/// </summary>
	class EditorContentBrowserPanel
	{
	public:
		static EditorContentBrowserPanel* GetInstance()
		{
			static EditorContentBrowserPanel instance;
			return &instance;
		}

		void Draw()
		{
#ifdef USE_IMGUI
			InitializeIfNeeded();

			// 旧Content Browserを停止し、同じDock IDへV2だけを登録する。
			EditorWindowManager::GetInstance()->GetWindowState().showContentBrowser = false;

			if (!ImGui::Begin("コンテンツブラウザ###Content Browser", nullptr, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			DrawToolbar();
			ImGui::Separator();
			DrawBrowserBody();
			ImGui::End();
#endif
		}

	private:
		EditorContentBrowserPanel() = default;
		~EditorContentBrowserPanel() = default;
		EditorContentBrowserPanel(const EditorContentBrowserPanel&) = delete;
		EditorContentBrowserPanel& operator=(const EditorContentBrowserPanel&) = delete;

#ifdef USE_IMGUI
		void InitializeIfNeeded()
		{
			if (initialized_)
			{
				return;
			}

			registry_.Initialize();
			history_.push_back({});
			historyIndex_ = 0;
			initialized_ = true;
		}

		void RefreshRegistry()
		{
			registry_.Refresh();
			if (!registry_.IsValidDirectory(currentDirectory_))
			{
				NavigateTo({}, true);
			}
			if (selectedAssetId_ != 0 && registry_.FindById(selectedAssetId_) == nullptr)
			{
				selectedAssetId_ = 0;
			}
		}

		void NavigateTo(const std::filesystem::path& directory, bool recordHistory)
		{
			const std::filesystem::path normalized = EditorAssetRegistryV2::NormalizeRelative(directory);
			if (!registry_.IsValidDirectory(normalized))
			{
				return;
			}

			if (currentDirectory_ == normalized)
			{
				return;
			}

			currentDirectory_ = normalized;
			selectedAssetId_ = 0;

			if (recordHistory)
			{
				if (historyIndex_ + 1 < history_.size())
				{
					history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1), history_.end());
				}
				history_.push_back(currentDirectory_);
				historyIndex_ = history_.size() - 1;
			}
		}

		bool CanNavigateBack() const
		{
			return historyIndex_ > 0 && !history_.empty();
		}

		bool CanNavigateForward() const
		{
			return !history_.empty() && historyIndex_ + 1 < history_.size();
		}

		void NavigateBack()
		{
			if (!CanNavigateBack())
			{
				return;
			}
			--historyIndex_;
			currentDirectory_ = history_[historyIndex_];
			selectedAssetId_ = 0;
		}

		void NavigateForward()
		{
			if (!CanNavigateForward())
			{
				return;
			}
			++historyIndex_;
			currentDirectory_ = history_[historyIndex_];
			selectedAssetId_ = 0;
		}

		void NavigateUp()
		{
			if (currentDirectory_.empty())
			{
				return;
			}
			NavigateTo(currentDirectory_.parent_path(), true);
		}

		void DrawToolbar()
		{
			ImGui::BeginDisabled();
			ImGui::Button("+ 追加");
			ImGui::SameLine();
			ImGui::Button("インポート");
			ImGui::EndDisabled();

			ImGui::SameLine();
			DrawNavigationButton("<", "戻る", CanNavigateBack(), [this]() { NavigateBack(); });
			ImGui::SameLine();
			DrawNavigationButton(">", "進む", CanNavigateForward(), [this]() { NavigateForward(); });
			ImGui::SameLine();
			DrawNavigationButton("^", "ひとつ上のフォルダ", !currentDirectory_.empty(), [this]() { NavigateUp(); });
			ImGui::SameLine();
			if (ImGui::Button("更新"))
			{
				RefreshRegistry();
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::BeginCombo("##AssetTypeFilter", EditorAssetRegistryV2::GetTypeName(typeFilter_)))
			{
				constexpr std::array<EditorAssetType, 12> filterTypes = {
					EditorAssetType::All,
					EditorAssetType::Texture,
					EditorAssetType::Model,
					EditorAssetType::Animation,
					EditorAssetType::Material,
					EditorAssetType::ActorPrefab,
					EditorAssetType::Level,
					EditorAssetType::Shader,
					EditorAssetType::Font,
					EditorAssetType::Audio,
					EditorAssetType::Json,
					EditorAssetType::Other,
				};
				for (const EditorAssetType type : filterTypes)
				{
					const bool selected = typeFilter_ == type;
					if (ImGui::Selectable(EditorAssetRegistryV2::GetTypeName(type), selected))
					{
						typeFilter_ = type;
						selectedAssetId_ = 0;
					}
					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat("##AssetCellSize", &cellWidth_, 88.0f, 176.0f, "表示 %.0f");
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("アセットの表示サイズを変更します。");
			}

			ImGui::Spacing();
			DrawBreadcrumbs();
			ImGui::SameLine();
			const float searchWidth = std::clamp(ImGui::GetContentRegionAvail().x, 180.0f, 420.0f);
			ImGui::SetNextItemWidth(searchWidth);
			ImGui::InputTextWithHint("##ContentBrowserV2Search", "現在のフォルダ以下を検索...", searchBuffer_.data(), searchBuffer_.size());
		}

		template <class Action>
		void DrawNavigationButton(const char* label, const char* tooltip, bool enabled, Action action)
		{
			if (!enabled)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button(label, ImVec2(30.0f, 0.0f)) && enabled)
			{
				action();
			}
			if (!enabled)
			{
				ImGui::EndDisabled();
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", tooltip);
			}
		}

		void DrawBreadcrumbs()
		{
			if (ImGui::Button("コンテンツ"))
			{
				NavigateTo({}, true);
			}

			std::filesystem::path accumulated;
			for (const std::filesystem::path& part : currentDirectory_)
			{
				accumulated /= part;
				ImGui::SameLine();
				ImGui::TextDisabled(">");
				ImGui::SameLine();
				const std::string label = part.generic_string() + "##Breadcrumb" + accumulated.generic_string();
				if (ImGui::Button(label.c_str()))
				{
					NavigateTo(accumulated, true);
				}
			}
		}

		void DrawBrowserBody()
		{
			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const bool showDetails = availableWidth >= 980.0f;
			const float treeWidth = std::clamp(availableWidth * 0.18f, 190.0f, 270.0f);
			const float detailsWidth = showDetails ? std::clamp(availableWidth * 0.22f, 260.0f, 360.0f) : 0.0f;

			if (ImGui::BeginChild("##ContentSources", ImVec2(treeWidth, 0.0f), true))
			{
				ImGui::TextUnformatted("ソース");
				ImGui::Separator();
				DrawFolderTree();
			}
			ImGui::EndChild();

			ImGui::SameLine();
			const float centerWidth = showDetails
				? -(detailsWidth + ImGui::GetStyle().ItemSpacing.x)
				: 0.0f;
			if (ImGui::BeginChild("##ContentAssets", ImVec2(centerWidth, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
			{
				DrawAssetGrid();
			}
			ImGui::EndChild();

			if (showDetails)
			{
				ImGui::SameLine();
				if (ImGui::BeginChild("##ContentDetails", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar))
				{
					DrawSelectedAssetDetails();
				}
				ImGui::EndChild();
			}
		}

		void DrawFolderTree()
		{
			ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen |
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			if (currentDirectory_.empty())
			{
				rootFlags |= ImGuiTreeNodeFlags_Selected;
			}
			const bool rootOpen = ImGui::TreeNodeEx("コンテンツ##ContentRoot", rootFlags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				NavigateTo({}, true);
			}
			if (rootOpen)
			{
				for (const EditorAssetData* directory : registry_.GetDirectories({}))
				{
					DrawFolderNode(*directory);
				}
				ImGui::TreePop();
			}
		}

		void DrawFolderNode(const EditorAssetData& directory)
		{
			const std::vector<const EditorAssetData*> children = registry_.GetDirectories(directory.relativePath);
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			if (children.empty())
			{
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (currentDirectory_ == directory.relativePath)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(directory.id)));
			const std::string label = "[DIR] " + directory.name;
			const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				NavigateTo(directory.relativePath, true);
			}
			if (!children.empty() && open)
			{
				for (const EditorAssetData* child : children)
				{
					DrawFolderNode(*child);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		void DrawAssetGrid()
		{
			const std::string_view search(searchBuffer_.data());
			const std::vector<const EditorAssetData*> entries = registry_.Query(currentDirectory_, search, typeFilter_);

			ImGui::Text("%s  (%zu項目)", currentDirectory_.empty() ? "コンテンツ" : currentDirectory_.generic_string().c_str(), entries.size());
			if (!search.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("サブフォルダを含む検索結果");
			}
			ImGui::Separator();

			if (!registry_.GetLastError().empty())
			{
				ImGui::TextWrapped("%s", registry_.GetLastError().c_str());
			}
			if (entries.empty())
			{
				ImGui::TextDisabled("表示できるアセットがありません。");
				return;
			}

			const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const int columnCount = std::max(1, static_cast<int>((availableWidth + spacing) / (cellWidth_ + spacing)));
			const float actualCellWidth = std::floor((availableWidth - spacing * static_cast<float>(columnCount - 1)) / static_cast<float>(columnCount));

			for (std::size_t index = 0; index < entries.size(); ++index)
			{
				DrawAssetCard(*entries[index], actualCellWidth, !search.empty());
				if ((static_cast<int>(index) + 1) % columnCount != 0)
				{
					ImGui::SameLine();
				}
			}
		}

		void DrawAssetCard(const EditorAssetData& asset, float width, bool showRelativePath)
		{
			const float previewHeight = std::max(52.0f, width * 0.58f);
			const float cardHeight = previewHeight + (showRelativePath ? 66.0f : 48.0f);
			const bool selected = selectedAssetId_ == asset.id;

			ImGui::PushID(reinterpret_cast<void*>(static_cast<uintptr_t>(asset.id)));
			ImGui::InvisibleButton("##AssetCard", ImVec2(width, cardHeight));
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				selectedAssetId_ = asset.id;
			}
			if (asset.isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				NavigateTo(asset.relativePath, true);
			}

			if (ImGui::IsItemVisible())
			{
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				const ImVec2 min = ImGui::GetItemRectMin();
				const ImVec2 max = ImGui::GetItemRectMax();
				const ImU32 background = ImGui::GetColorU32(selected ? ImVec4(0.20f, 0.33f, 0.52f, 0.95f) : ImVec4(0.09f, 0.10f, 0.12f, 0.95f));
				const ImU32 border = ImGui::GetColorU32(selected ? ImVec4(0.95f, 0.53f, 0.08f, 1.0f) : ImVec4(0.24f, 0.25f, 0.29f, 1.0f));
				drawList->AddRectFilled(min, max, background, 4.0f);
				drawList->AddRect(min, max, border, 4.0f, 0, selected ? 2.0f : 1.0f);

				const char* badge = EditorAssetRegistryV2::GetTypeBadge(asset.type);
				const ImVec2 badgeSize = ImGui::CalcTextSize(badge);
				const ImVec2 previewMin(min.x + 7.0f, min.y + 7.0f);
				const ImVec2 previewMax(max.x - 7.0f, min.y + previewHeight);
				drawList->AddRectFilled(previewMin, previewMax, ImGui::GetColorU32(ImVec4(0.14f, 0.15f, 0.18f, 1.0f)), 3.0f);
				drawList->AddText(
					ImVec2((previewMin.x + previewMax.x - badgeSize.x) * 0.5f, (previewMin.y + previewMax.y - badgeSize.y) * 0.5f),
					ImGui::GetColorU32(asset.isDirectory ? ImVec4(0.95f, 0.72f, 0.30f, 1.0f) : ImVec4(0.72f, 0.84f, 1.0f, 1.0f)),
					badge);

				const std::string displayName = TruncateText(asset.name, width - 14.0f);
				drawList->AddText(ImVec2(min.x + 7.0f, previewMax.y + 7.0f), ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());
				if (showRelativePath)
				{
					const std::string pathText = TruncateText(asset.parentPath.generic_string(), width - 14.0f);
					drawList->AddText(ImVec2(min.x + 7.0f, previewMax.y + 27.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), pathText.c_str());
				}
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s\n種類: %s\nパス: %s%s",
					asset.name.c_str(),
					EditorAssetRegistryV2::GetTypeName(asset.type),
					asset.relativePath.generic_string().c_str(),
					asset.isDirectory ? "\nダブルクリックで開く" : "");
			}
			ImGui::PopID();
		}

		void DrawSelectedAssetDetails()
		{
			ImGui::TextUnformatted("選択中のアセット");
			ImGui::Separator();

			const EditorAssetData* asset = registry_.FindById(selectedAssetId_);
			if (!asset)
			{
				ImGui::TextDisabled("アセットが選択されていません。");
				return;
			}

			DrawDetailRow("名前", asset->name);
			DrawDetailRow("種類", EditorAssetRegistryV2::GetTypeName(asset->type));
			DrawDetailRow("相対パス", asset->relativePath.generic_string());
			DrawDetailRow("更新日時", asset->modifiedTime);
			if (asset->isDirectory)
			{
				DrawDetailRow("項目数", std::to_string(registry_.CountChildren(asset->relativePath)));
				if (ImGui::Button("このフォルダを開く", ImVec2(-1.0f, 0.0f)))
				{
					NavigateTo(asset->relativePath, true);
				}
			}
			else
			{
				DrawDetailRow("拡張子", asset->extension.empty() ? "なし" : asset->extension);
				DrawDetailRow("サイズ", FormatBytes(asset->sizeBytes));
				DrawDetailRow("絶対パス", asset->absolutePath.generic_string());
			}
		}

		static void DrawDetailRow(const char* label, const std::string& value)
		{
			ImGui::TextDisabled("%s", label);
			ImGui::TextWrapped("%s", value.empty() ? "なし" : value.c_str());
			ImGui::Spacing();
		}

		static std::string FormatBytes(uintmax_t bytes)
		{
			std::ostringstream stream;
			if (bytes >= 1024ull * 1024ull)
			{
				stream.setf(std::ios::fixed);
				stream.precision(2);
				stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
			}
			else if (bytes >= 1024ull)
			{
				stream.setf(std::ios::fixed);
				stream.precision(2);
				stream << static_cast<double>(bytes) / 1024.0 << " KB";
			}
			else
			{
				stream << bytes << " bytes";
			}
			return stream.str();
		}

		static std::string TruncateText(const std::string& text, float maxWidth)
		{
			if (text.empty() || ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
			{
				return text;
			}

			std::string result;
			for (const char character : text)
			{
				const std::string candidate = result + character;
				if (ImGui::CalcTextSize((candidate + "...").c_str()).x > maxWidth)
				{
					break;
				}
				result = candidate;
			}
			return result + "...";
		}
#endif

		EditorAssetRegistryV2 registry_{};
		std::filesystem::path currentDirectory_{};
		std::vector<std::filesystem::path> history_{};
		std::size_t historyIndex_ = 0;
		uint64_t selectedAssetId_ = 0;
		EditorAssetType typeFilter_ = EditorAssetType::All;
		std::array<char, 160> searchBuffer_{};
		float cellWidth_ = 128.0f;
		bool initialized_ = false;
	};
} // namespace Ken4lowEngine
