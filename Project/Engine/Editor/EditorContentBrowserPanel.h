#pragma once

#include "EditorAssetDragDrop.h"
#include "EditorAssetRegistryV2.h"
#include "EditorPlayController.h"
#include "EditorTexturePreviewCache.h"
#include "EditorViewportController.h"
#include "EditorViewportPicking.h"
#include "EditorWindowManager.h"

#include <SceneManager.h>

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
		static EditorContentBrowserPanel* GetInstance();

		void Draw();

		/// <summary>
		/// 選択中のモデルまたはActor PrefabをMain Viewportへ渡すDrag Sourceを更新します。
		/// </summary>
		void UpdateAssetDragSource()
		{
#ifdef USE_IMGUI
			UpdateViewportPicking(); // Content Browserの選択状態に関係なくMain Viewportクリックを先に処理する。

			const EditorAssetData* asset = registry_.FindById(selectedAssetId_);
			if (!asset || asset->isDirectory || !IsViewportPlaceableAsset(asset->type))
			{
				return;
			}

			// 同じContent Browserウィンドウへ再度入って、子領域上のドラッグを外部Sourceとして登録する。
			if (!ImGui::Begin("コンテンツブラウザ###Content Browser", nullptr, ImGuiWindowFlags_NoCollapse))
			{
				ImGui::End();
				return;
			}

			const bool canStartDrag =
				ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
				ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f);

			if (canStartDrag && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern))
			{
				const EditorAssetDragDropPayload payload = MakeEditorAssetDragDropPayload(*asset);
				ImGui::SetDragDropPayload(kEditorAssetDragDropPayloadType, &payload, sizeof(payload));
				ImGui::TextUnformatted("ビューポートへ配置");
				ImGui::Text("%s", asset->name.c_str());
				ImGui::TextDisabled("種類: %s", EditorAssetRegistryV2::GetTypeName(asset->type));
				ImGui::EndDragDropSource();
			}
			ImGui::End();
#endif
		}

	private:
		enum class AssetSortMode
		{
			Name,
			ModifiedTime,
			Size,
		};

		EditorContentBrowserPanel() = default;
		~EditorContentBrowserPanel() = default;
		EditorContentBrowserPanel(const EditorContentBrowserPanel&) = delete;
		EditorContentBrowserPanel& operator=(const EditorContentBrowserPanel&) = delete;

#ifdef USE_IMGUI
		void UpdateViewportPicking()
		{
			auto* viewportController = EditorViewportController::GetInstance();
			if (!viewportController->IsEditorDisplay() || viewportController->GetTool() != EditorViewportTool::Select ||
				EditorPlayController::GetInstance()->IsPlaying() || ImGui::GetDragDropPayload() != nullptr ||
				!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				return;
			}

			auto* windowManager = EditorWindowManager::GetInstance();
			const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
			if (!viewportRect.valid)
			{
				return;
			}

			const ImVec2 mouse = ImGui::GetMousePos();
			const bool insideViewport =
				mouse.x >= viewportRect.screenMin.x && mouse.y >= viewportRect.screenMin.y &&
				mouse.x <= viewportRect.screenMax.x && mouse.y <= viewportRect.screenMax.y;
			const bool insideToolbar = mouse.y <= viewportRect.screenMin.y + 58.0f;
			if (!insideViewport || insideToolbar)
			{
				return;
			}

			SceneManager* sceneManager = windowManager->GetSceneManager();
			BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			if (!scene)
			{
				return;
			}

			EditorViewportPickingRay ray{};
			if (!EditorViewportPicking::BuildRay(
				{ mouse.x, mouse.y },
				viewportRect.screenMin,
				viewportRect.imageSize,
				ray))
			{
				windowManager->AddOutputLog(EditorLogLevel::Warning, "Viewport Picking用Rayを作成できませんでした。");
				return;
			}

			std::vector<EditorObjectInfo> objects;
			scene->CollectEditorObjects(objects);
			EditorObjectInfo hitObject{};
			float hitDistance = 0.0f;
			if (EditorViewportPicking::PickClosest(ray, objects, hitObject, &hitDistance))
			{
				EditorContext::GetInstance()->GetSelection().Select(hitObject);
				windowManager->AddOutputLog(
					EditorLogLevel::Info,
					"Viewport選択: " + hitObject.displayName + " / ID=" + std::to_string(hitObject.id));
			}
			else
			{
				EditorContext::GetInstance()->GetSelection().Clear();
				windowManager->AddOutputLog(EditorLogLevel::Info, "Viewportの空き領域を選択したため選択を解除しました。");
			}
		}

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
		void DrawAssetContextMenu(const EditorAssetData& asset);
		void DrawPendingDialogs();
		void DrawSelectedAssetDetails();

		std::vector<const EditorAssetData*> BuildVisibleEntries() const;
		void OpenAsset(const EditorAssetData& asset) const;
		void RevealInExplorer(const EditorAssetData& asset) const;
		void OpenCurrentFolderInExplorer() const;
		void CopyPathToClipboard(const EditorAssetData& asset) const;
		void BeginRename(const EditorAssetData& asset);
		void BeginDelete(const EditorAssetData& asset);
		void ProcessDeferredDuplicate();
		void ApplyRename();
		void ApplyDelete();
		void DuplicateAsset(const EditorAssetData& asset);
		std::filesystem::path MakeUniqueCopyPath(const EditorAssetData& asset) const;
		void SetOperationResult(std::string message, bool failed);

		static void DrawDetailRow(const char* label, const std::string& value);
		static std::string FormatBytes(uintmax_t bytes);
		static std::string TruncateText(const std::string& text, float maxWidth);
		static const char* GetSortModeName(AssetSortMode mode);
#endif

	private:
		EditorAssetRegistryV2 registry_{};
		EditorTexturePreviewCache texturePreviewCache_{}; // 表示中のテクスチャだけをSRV化してサムネイルへ再利用する。
		std::filesystem::path currentDirectory_{};
		std::vector<std::filesystem::path> history_{};
		std::size_t historyIndex_ = 0;
		uint64_t selectedAssetId_ = 0;
		EditorAssetType typeFilter_ = EditorAssetType::All;
		AssetSortMode sortMode_ = AssetSortMode::Name;
		std::array<char, 160> searchBuffer_{};
		std::array<char, 260> renameBuffer_{};
		float cellWidth_ = 128.0f;
		bool initialized_ = false;
		bool searchSubfolders_ = true;
		bool showFolders_ = true;
		bool sortAscending_ = true;
		uint64_t renameAssetId_ = 0;
		uint64_t deleteAssetId_ = 0;
		uint64_t duplicateAssetId_ = 0;
		bool openRenamePopup_ = false;
		bool openDeletePopup_ = false;
		bool duplicateRequested_ = false;
		std::string operationMessage_;
		bool operationFailed_ = false;
	};
} // namespace Ken4lowEngine
