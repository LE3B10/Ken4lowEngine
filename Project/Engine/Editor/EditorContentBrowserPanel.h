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



namespace Ken4lowEngine
{
	/// <summary>
	/// UE風のフォルダツリー・パンくず・アセットグリッドを持つコンテンツブラウザです。
	/// </summary>
	class EditorContentBrowserPanel
	{
	public:
		static EditorContentBrowserPanel* GetInstance();

		void Draw();

	private:
		EditorContentBrowserPanel() = default;
		~EditorContentBrowserPanel() = default;
		EditorContentBrowserPanel(const EditorContentBrowserPanel&) = delete;
		EditorContentBrowserPanel& operator=(const EditorContentBrowserPanel&) = delete;

#ifdef USE_IMGUI
		void InitializeIfNeeded();
		void RefreshRegistry();
		void NavigateTo(const std::filesystem::path& directory, bool recordHistory);

		bool CanNavigateBack() const
		{
			return historyIndex_ > 0 && !history_.empty();
		}

		bool CanNavigateForward() const
		{
			return !history_.empty() && historyIndex_ + 1 < history_.size();
		}

		void NavigateBack();
		void NavigateForward();
		void NavigateUp();

		void DrawToolbar();

		void DrawBreadcrumbs();

		void DrawBrowserBody();

		void DrawFolderTree();

		void DrawFolderNode(const EditorAssetData& directory);

		void DrawAssetGrid();

		void DrawAssetCard(const EditorAssetData& asset, float width, bool showRelativePath);

		void DrawSelectedAssetDetails();

		static void DrawDetailRow(const char* label, const std::string& value);

		static std::string FormatBytes(uintmax_t bytes);

		static std::string TruncateText(const std::string& text, float maxWidth);
#endif
	private:
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
