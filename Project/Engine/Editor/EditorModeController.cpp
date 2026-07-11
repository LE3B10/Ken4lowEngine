#include "EditorModeController.h"

#include "EditorPlayController.h"
#include "EditorWindowManager.h"
#include <Input.h>
#include <Wireframe.h>

#include <string>

namespace Ken4lowEngine
{
	EditorModeController* EditorModeController::GetInstance()
	{
		static EditorModeController instance;
		return &instance;
	}

	void EditorModeController::Initialize()
	{
#ifdef _DEBUG
		// Debug起動直後は従来通りEditor Modeから始める。
		editorModeEnabled_ = true;
#else
		// ReleaseではEditor機能を開かず、常にゲーム画面確認相当にする。
		editorModeEnabled_ = false;
#endif
		ApplyModeSideEffects();
	}

	void EditorModeController::Update(Input* input)
	{
#ifdef _DEBUG
		if (input != nullptr && input->TriggerRawKey(DIK_F1))
		{
			// F1はゲーム操作と競合しにくいDebug専用ショートカットとして扱う。
			SetEditorModeEnabled(!editorModeEnabled_);
		}

		EditorPlayController* playController = EditorPlayController::GetInstance();
		if (input != nullptr && IsEditorModeEnabled() && playController->IsPlaying() &&
			playController->IsGameCaptured() && input->TriggerRawKey(DIK_ESCAPE))
		{
			playController->ReleaseGameInput();
			input->SetGameInputEnabled(false);
			input->SetLockCursor(false);
			input->SetCursorVisible(true); // Scene更新より先にEscを処理し、Play状態を維持したままEditorへ入力を戻す。
		}
#else
		(void)input;
		SetEditorModeEnabled(false);
#endif
	}

	void EditorModeController::SetEditorModeEnabled(bool enabled)
	{
#ifndef _DEBUG
		enabled = false;
#endif
		if (editorModeEnabled_ == enabled)
		{
			ApplyModeSideEffects();
			return;
		}

		editorModeEnabled_ = enabled;
		ApplyModeSideEffects();
		NotifyModeChanged();
	}

	bool EditorModeController::IsEditorModeEnabled() const
	{
#ifdef _DEBUG
		return editorModeEnabled_;
#else
		return false;
#endif
	}

	void EditorModeController::ApplyModeSideEffects()
	{
		// Debug表示の最終入口をモードと同期し、Game Preview中はWireframe/Collider線を破棄する。
		Wireframe::GetInstance()->SetDebugDrawEnabled(ShouldDrawDebugVisuals());

		if (Input* input = Input::GetInstance())
		{
			if (IsGamePreviewMode())
			{
				// Preview中はEditor Viewport由来の座標補正と入力抑制を解除し、ゲーム入力を優先する。
				input->SetGameInputEnabled(true);
				input->ClearEditorViewportMouseOverride();
			}
		}
	}

	void EditorModeController::NotifyModeChanged() const
	{
#ifdef USE_IMGUI
		const char* modeName = IsEditorModeEnabled() ? "Editor Mode" : "Game Preview Mode";
		EditorWindowManager::GetInstance()->AddOutputLog(EditorLogLevel::Info, std::string("[Editor] Switched to ") + modeName + " (F1).");
#endif
	}

} // namespace Ken4lowEngine
