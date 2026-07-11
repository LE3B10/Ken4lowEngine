#define NOMINMAX
#include "EditorTransformGizmo.h"

#ifdef USE_IMGUI
#include "EditorContext.h"
#include "EditorModeController.h"
#include "EditorPlayController.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <CameraManager.h>
#include <DebugCamera.h>
#include <Matrix4x4.h>
#include <imgui.h>
#include <Externals/ImGuizmo/ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{
	namespace
	{
		ImGuizmo::OPERATION ToImGuizmoOperation(EditorViewportTool tool)
		{
			switch (tool)
			{
			case EditorViewportTool::Rotate: return ImGuizmo::ROTATE;
			case EditorViewportTool::Scale: return ImGuizmo::SCALE;
			case EditorViewportTool::Translate:
			default: return ImGuizmo::TRANSLATE;
			}
		}

		float KeepScaleInvertible(float value)
		{
			if (!std::isfinite(value))
			{
				return 1.0f;
			}
			if (std::abs(value) >= 0.001f)
			{
				return value;
			}
			return value < 0.0f ? -0.001f : 0.001f;
		}

		void UpdateToolShortcuts(EditorViewportController& controller)
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.WantTextInput || ImGui::IsAnyItemActive())
			{
				return;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) controller.SetTool(EditorViewportTool::Select);
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) controller.SetTool(EditorViewportTool::Translate);
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) controller.SetTool(EditorViewportTool::Rotate);
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) controller.SetTool(EditorViewportTool::Scale);
		}

		void FocusDebugCamera(const EditorObjectInfo& selected)
		{
			EditorTransform worldTransform{};
			CameraManager* cameraManager = CameraManager::GetInstance();
			DebugCamera* debugCamera = cameraManager->GetDebugCamera();
			if (!cameraManager->IsUsingDebugCamera() || !debugCamera || !selected.TryReadWorldTransform(worldTransform))
			{
				return; // Game CameraをEditor操作で動かさず、Debug Camera使用時だけF Focusを有効にする。
			}

			const Vector3 forward = cameraManager->GetActiveCameraForward();
			debugCamera->SetTranslate(worldTransform.position - forward * 8.0f);
		}
	}

	EditorTransformGizmo* EditorTransformGizmo::GetInstance()
	{
		static EditorTransformGizmo instance;
		return &instance;
	}

	void EditorTransformGizmo::Draw()
	{
		if (!EditorModeController::GetInstance()->IsEditorModeEnabled() || EditorPlayController::GetInstance()->IsPlaying())
		{
			return;
		}

		EditorViewportController& viewportController = *EditorViewportController::GetInstance();
		if (!viewportController.IsEditorDisplay())
		{
			return;
		}
		UpdateToolShortcuts(viewportController);

		EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
		if (!selection.HasSelection())
		{
			return;
		}

		const EditorObjectInfo& selected = selection.GetSelected();
		if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive())
		{
			FocusDebugCamera(selected);
		}
		if (viewportController.GetTool() == EditorViewportTool::Select || !selected.canEditTransform)
		{
			return;
		}

		const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
		if (!viewportRect.valid || viewportRect.imageSize.x <= 1.0f || viewportRect.imageSize.y <= 1.0f)
		{
			return;
		}

		EditorTransform worldTransform{};
		if (!selected.TryReadWorldTransform(worldTransform))
		{
			return;
		}

		Matrix4x4 transformMatrix = Matrix4x4::MakeAffineMatrix(
			worldTransform.scale,
			worldTransform.rotation,
			worldTransform.position);
		const Matrix4x4 view = CameraManager::GetInstance()->GetActiveViewMatrix();
		const Matrix4x4 projection = CameraManager::GetInstance()->GetActiveProjectionMatrix();

		ImGuizmo::BeginFrame();
		ImGuizmo::Enable(true);
		ImGuizmo::AllowAxisFlip(false);
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		ImGuizmo::SetRect(
			viewportRect.screenMin.x,
			viewportRect.screenMin.y,
			viewportRect.imageSize.x,
			viewportRect.imageSize.y);

		const EditorViewportTool activeTool = viewportController.GetTool();
		const ImGuizmo::OPERATION operation = ToImGuizmoOperation(activeTool);
		const ImGuizmo::MODE mode = activeTool == EditorViewportTool::Scale ||
			viewportController.GetGizmoSpace() == EditorGizmoSpace::Local
			? ImGuizmo::LOCAL
			: ImGuizmo::WORLD;

		float snap[3] = {};
		const float* snapValues = nullptr;
		if (viewportController.IsSnapEnabled())
		{
			if (activeTool == EditorViewportTool::Translate)
			{
				const Vector3& translationSnap = viewportController.GetTranslationSnap();
				snap[0] = std::max(0.001f, translationSnap.x);
				snap[1] = std::max(0.001f, translationSnap.y);
				snap[2] = std::max(0.001f, translationSnap.z);
			}
			else if (activeTool == EditorViewportTool::Rotate)
			{
				snap[0] = std::max(0.1f, viewportController.GetRotationSnapDegrees());
			}
			else
			{
				snap[0] = std::max(0.001f, viewportController.GetScaleSnap());
				snap[1] = snap[0];
				snap[2] = snap[0];
			}
			snapValues = snap;
		}

		const bool changed = ImGuizmo::Manipulate(
			&view.m[0][0],
			&projection.m[0][0],
			operation,
			mode,
			&transformMatrix.m[0][0],
			nullptr,
			snapValues);
		if (!changed)
		{
			return;
		}

		float translation[3] = {};
		float rotationDegrees[3] = {};
		float scale[3] = {};
		ImGuizmo::DecomposeMatrixToComponents(
			&transformMatrix.m[0][0],
			translation,
			rotationDegrees,
			scale);

		constexpr float toRadians = std::numbers::pi_v<float> / 180.0f;
		EditorTransform editedWorldTransform{};
		editedWorldTransform.position = { translation[0], translation[1], translation[2] };
		editedWorldTransform.rotation = {
			rotationDegrees[0] * toRadians,
			rotationDegrees[1] * toRadians,
			rotationDegrees[2] * toRadians,
		};
		editedWorldTransform.scale = {
			KeepScaleInvertible(scale[0]),
			KeepScaleInvertible(scale[1]),
			KeepScaleInvertible(scale[2]),
		};

		selected.WriteWorldTransform(editedWorldTransform);
		EditorContext::GetInstance()->MarkLevelDirty();
	}

	bool EditorTransformGizmo::IsUsing() const
	{
		return ImGuizmo::IsUsing();
	}
} // namespace Ken4lowEngine
#endif // USE_IMGUI
