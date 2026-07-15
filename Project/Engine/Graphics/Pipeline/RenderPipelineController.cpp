#include "RenderPipelineController.h"

#include "DirectXCommon.h"
#include "GameTimer.h"
#include "LightManager.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	RenderPipelineController* RenderPipelineController::activeController_ = nullptr;

	void RenderPipelineController::Initialize(DirectXCommon* dxCommon)
	{
		// ControllerはDX12リソースを所有せず、既存DirectXCommonのフレーム入口だけを参照する。
		dxCommon_ = dxCommon;
		activeController_ = this;
	}

	void RenderPipelineController::ExecuteFrame(bool editorModeEnabled, const FrameCallbacks& callbacks)
	{
		if (!dxCommon_)
		{
			return;
		}

		MeasurePhase(PerformancePhase::BeginDraw, [this]()
			{
				dxCommon_->BeginDraw(); // BackBuffer index確定を含むCPU待ち時間も独立して計測する。
			});

		ExecuteShadowMapPass(callbacks);

		if (editorModeEnabled)
		{
			ExecuteEditorFrame(callbacks);
		}
		else
		{
			ExecuteGameFrame(callbacks);
		}
	}

	void RenderPipelineController::MeasurePhase(PerformancePhase phase, const std::function<void()>& callback)
	{
		if (!callback)
		{
			return;
		}

		const auto begin = Clock::now();
		callback();
		const float elapsedMs = std::chrono::duration<float, std::milli>(Clock::now() - begin).count();
		UpdatePerformanceMetric(phase, elapsedMs);
	}

	void RenderPipelineController::UpdatePerformanceMetric(PerformancePhase phase, float elapsedMs)
	{
		PerformanceMetric& metric = performanceMetrics_[ToIndex(phase)];
		metric.lastMs = (std::max)(0.0f, elapsedMs);
		metric.averageMs = metric.sampleCount == 0
			? metric.lastMs
			: metric.averageMs * 0.9f + metric.lastMs * 0.1f;
		metric.maxMs = (std::max)(metric.maxMs, metric.lastMs);
		++metric.sampleCount;
	}

	const RenderPipelineController::PerformanceMetric& RenderPipelineController::GetPerformanceMetric(PerformancePhase phase) const
	{
		return performanceMetrics_[ToIndex(phase)];
	}

	const char* RenderPipelineController::GetPerformancePhaseName(PerformancePhase phase)
	{
		switch (phase)
		{
		case PerformancePhase::BeginDraw: return "BeginDraw";
		case PerformancePhase::ShadowPrepare: return "Shadow Prepare";
		case PerformancePhase::ShadowRender: return "Shadow Render";
		case PerformancePhase::EditorUiBuild: return "Editor UI Build";
		case PerformancePhase::EditorPicking: return "Editor Picking";
		case PerformancePhase::MainWorldRender: return "Main World Render";
		case PerformancePhase::PostEffect: return "PostEffect";
		case PerformancePhase::SelectionOutline: return "Selection Outline";
		case PerformancePhase::SceneOverlay: return "Scene Overlay";
		case PerformancePhase::ImGuiRender: return "ImGui Render";
		case PerformancePhase::BackBufferPostEffect: return "BackBuffer PostEffect";
		case PerformancePhase::BackBufferRebind: return "BackBuffer Rebind";
		case PerformancePhase::GameUi: return "Game UI";
		default: return "Unknown";
		}
	}

	void RenderPipelineController::DrawPerformanceImGui()
	{
#ifdef USE_IMGUI
		const GameTimer::CompletedFrameTiming& completed = GameTimer::GetInstance()->GetCompletedFrameTiming();
		frameTimingSummary_.frameIntervalMs = completed.frameIntervalMs;
		frameTimingSummary_.updateMs = completed.updateMs;
		frameTimingSummary_.drawMs = completed.drawMs;
		frameTimingSummary_.presentMs = completed.presentMs;
		frameTimingSummary_.totalFrameMs = completed.totalFrameMs;

		if (!ImGui::Begin("Render Pipeline Performance"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::Button("最大値リセット"))
		{
			for (PerformanceMetric& metric : performanceMetrics_)
			{
				metric.maxMs = metric.lastMs;
			}
		}

		ImGui::SeparatorText("前回完了フレーム");
		ImGui::Text("Frame Interval: %.2f ms", frameTimingSummary_.frameIntervalMs);
		ImGui::Text("Update: %.3f ms", frameTimingSummary_.updateMs);
		ImGui::Text("Draw: %.3f ms", frameTimingSummary_.drawMs);
		ImGui::Text("Present / GPU Wait: %.3f ms", frameTimingSummary_.presentMs);
		ImGui::Text("Total Frame: %.3f ms", frameTimingSummary_.totalFrameMs);

		const float accountedMs = frameTimingSummary_.updateMs + frameTimingSummary_.drawMs + frameTimingSummary_.presentMs;
		ImGui::Text("Unaccounted: %.3f ms", (std::max)(0.0f, frameTimingSummary_.frameIntervalMs - accountedMs));

		ImGui::SeparatorText("Render Pipeline CPU Pass");
		if (ImGui::BeginTable("##RenderPipelinePerformanceTable", 4,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("Phase");
			ImGui::TableSetupColumn("Last ms");
			ImGui::TableSetupColumn("EMA ms");
			ImGui::TableSetupColumn("Max ms");
			ImGui::TableHeadersRow();

			for (std::size_t index = 0; index < kPerformancePhaseCount; ++index)
			{
				const PerformancePhase phase = static_cast<PerformancePhase>(index);
				const PerformanceMetric& metric = performanceMetrics_[index];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(GetPerformancePhaseName(phase));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", metric.lastMs);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", metric.averageMs);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.3f", metric.maxMs);
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("判定");
		const float presentMs = frameTimingSummary_.presentMs;
		const float editorUiMs = GetPerformanceMetric(PerformancePhase::EditorUiBuild).averageMs;
		const float mainWorldMs = GetPerformanceMetric(PerformancePhase::MainWorldRender).averageMs;
		const float shadowMs = GetPerformanceMetric(PerformancePhase::ShadowRender).averageMs;

		if (presentMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Present/GPU待ちが大きいです。GPU側またはVSync/同期を優先して調査します。");
		}
		else if (editorUiMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Editor UI Buildが重いです。Outliner/Inspectorの全件再構築を優先して調査します。");
		}
		else if (mainWorldMs + shadowMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Main World + Shadowが重いです。DrawCallとMesh描画回数を優先して調査します。");
		}
		else
		{
			ImGui::TextDisabled("大きなCPU Passが見えない場合はGPUタイムスタンプ計測を次に追加します。");
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	void RenderPipelineController::ExecuteShadowMapPass(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::ShadowPrepare, callbacks.prepareShadowPass);

		MeasurePhase(PerformancePhase::ShadowRender, [&callbacks]()
			{
				// 通常描画より前という順序は維持し、選択ライトに応じたSlice描画時間をまとめて計測する。
				LightManager::GetInstance()->ExecuteShadowPasses(callbacks.drawShadowObjects);
			});
	}

	void RenderPipelineController::ExecuteEditorFrame(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::EditorUiBuild, callbacks.buildEditorUi);
		MeasurePhase(PerformancePhase::EditorPicking, callbacks.executeEditorPickingPass);
		MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget);
		MeasurePhase(PerformancePhase::PostEffect, callbacks.renderPostEffectToGameRenderTarget);
		MeasurePhase(PerformancePhase::SelectionOutline, callbacks.renderEditorSelectionOutline);

		MeasurePhase(PerformancePhase::SceneOverlay, [&callbacks]()
			{
				if (callbacks.beginGameRenderTargetOverlay) callbacks.beginGameRenderTargetOverlay();
				if (callbacks.drawScene2DOverlay) callbacks.drawScene2DOverlay();
				if (callbacks.endGameRenderTargetOverlay) callbacks.endGameRenderTargetOverlay();
			});

		MeasurePhase(PerformancePhase::ImGuiRender, callbacks.drawImGuiOverlay);
	}

	void RenderPipelineController::ExecuteGameFrame(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget);
		MeasurePhase(PerformancePhase::BackBufferPostEffect, callbacks.applyPostEffectToBackBuffer);
		MeasurePhase(PerformancePhase::BackBufferRebind, callbacks.rebindBackBufferForGameOverlay);
		MeasurePhase(PerformancePhase::GameUi, callbacks.drawGameUIToBackBuffer);
	}
} // namespace Ken4lowEngine
