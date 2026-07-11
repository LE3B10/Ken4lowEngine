#pragma once

#include <cstdint>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// メインビューポートに表示する内容を、編集確認用とゲーム確認用で切り替えます。
	/// </summary>
	enum class EditorViewportDisplayMode : uint8_t
	{
		Editor = 0,
		Game,
	};

	/// <summary>
	/// ビューポート上部ツールバーで選択する操作モードです。
	/// 実際のギズモ操作はPhase 8で接続します。
	/// </summary>
	enum class EditorViewportTool : uint8_t
	{
		Select = 0,
		Translate,
		Rotate,
		Scale,
	};

	/// <summary>
	/// Object-ID Passの結果をActorまたはComponentのどちらとして選択するかを表します。
	/// </summary>
	enum class EditorViewportSelectionMode : uint8_t
	{
		Actor = 0,
		Component,
	};

	/// <summary>
	/// ビューポートツールバーの表示状態と、Editor/Game表示の切り替え状態を管理します。
	/// </summary>
	class EditorViewportController
	{
	public:
		static EditorViewportController* GetInstance()
		{
			static EditorViewportController instance;
			return &instance;
		}

		void SetDisplayMode(EditorViewportDisplayMode mode) { displayMode_ = mode; }
		EditorViewportDisplayMode GetDisplayMode() const { return displayMode_; }
		bool IsEditorDisplay() const { return displayMode_ == EditorViewportDisplayMode::Editor; }
		bool IsGameDisplay() const { return displayMode_ == EditorViewportDisplayMode::Game; }

		void SetTool(EditorViewportTool tool) { activeTool_ = tool; }
		EditorViewportTool GetTool() const { return activeTool_; }

		void SetSelectionMode(EditorViewportSelectionMode mode) { selectionMode_ = mode; }
		EditorViewportSelectionMode GetSelectionMode() const
		{
#ifdef USE_IMGUI
			if (ImGui::GetCurrentContext() && ImGui::GetIO().KeyCtrl)
			{
				return EditorViewportSelectionMode::Component; // Ctrlを押しているクリックだけComponent選択へ一時切り替えする。
			}
#endif
			return selectionMode_;
		}

		void SetAuxiliaryDisplayEnabled(bool enabled) { auxiliaryDisplayEnabled_ = enabled; }
		bool IsAuxiliaryDisplayEnabled() const { return auxiliaryDisplayEnabled_; }

		const char* GetDisplayModeText() const
		{
			return IsGameDisplay() ? "ゲーム表示" : "エディター表示";
		}

		const char* GetSelectionModeText() const
		{
			return selectionMode_ == EditorViewportSelectionMode::Component ? "コンポーネント" : "アクタ";
		}

		const char* GetToolText() const
		{
			switch (activeTool_)
			{
			case EditorViewportTool::Translate:
				return "移動";
			case EditorViewportTool::Rotate:
				return "回転";
			case EditorViewportTool::Scale:
				return "拡縮";
			case EditorViewportTool::Select:
			default:
				return "選択";
			}
		}

	private:
		EditorViewportController() = default;
		~EditorViewportController() = default;
		EditorViewportController(const EditorViewportController&) = delete;
		EditorViewportController& operator=(const EditorViewportController&) = delete;

		EditorViewportDisplayMode displayMode_ = EditorViewportDisplayMode::Editor;
		EditorViewportTool activeTool_ = EditorViewportTool::Select;
		EditorViewportSelectionMode selectionMode_ = EditorViewportSelectionMode::Actor;
		bool auxiliaryDisplayEnabled_ = true;
	};
} // namespace Ken4lowEngine
