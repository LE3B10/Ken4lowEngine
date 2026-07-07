#include "RenderPipelineController.h"

#include "DirectXCommon.h"

namespace Ken4lowEngine
{
	void RenderPipelineController::Initialize(DirectXCommon* dxCommon)
	{
		// ControllerはDX12リソースを所有せず、既存DirectXCommonのフレーム入口だけを参照する。
		dxCommon_ = dxCommon;
	}

	void RenderPipelineController::ExecuteFrame(bool editorModeEnabled, const FrameCallbacks& callbacks)
	{
		if (!dxCommon_)
		{
			return;
		}

		// フレーム開始は従来通りDirectXCommonに任せ、BackBuffer indexなどの低レベル状態を確定させる。
		dxCommon_->BeginDraw();

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

	void RenderPipelineController::ExecuteShadowMapPass(const FrameCallbacks& callbacks)
	{
		// ShadowMapは通常描画より前に作る。順序を変えず、Begin/Draw/Endだけをここへ集約する。
		dxCommon_->BeginShadowMapPass();
		if (callbacks.drawShadowObjects)
		{
			callbacks.drawShadowObjects();
		}
		dxCommon_->EndShadowMapPass();
	}

	void RenderPipelineController::ExecuteEditorFrame(const FrameCallbacks& callbacks)
	{
		// Editor UIは既存通り先にImGuiコマンドを組み、最後にBackBufferへOverlayとして描画する。
		if (callbacks.buildEditorUi)
		{
			callbacks.buildEditorUi();
		}

		// 3D/ParticleはSceneRenderTargetへ描き、PostEffectの入力を従来と同じ形で作る。
		if (callbacks.drawGameWorldToSceneTarget)
		{
			callbacks.drawGameWorldToSceneTarget();
		}

		// Editor ModeではPostEffect結果をMain Viewport用GameRenderTargetへ集約する。
		if (callbacks.renderPostEffectToGameRenderTarget)
		{
			callbacks.renderPostEffectToGameRenderTarget();
		}

		// HUD/UI/SpriteはMain Viewportに含めるため、PostEffect後のGameRenderTargetへ直接重ねる。
		if (callbacks.beginGameRenderTargetOverlay)
		{
			callbacks.beginGameRenderTargetOverlay();
		}
		if (callbacks.drawScene2DOverlay)
		{
			callbacks.drawScene2DOverlay();
		}
		if (callbacks.endGameRenderTargetOverlay)
		{
			callbacks.endGameRenderTargetOverlay();
		}

		// 最後にImGuiをBackBufferへ描画し、EditorウィンドウとMain Viewportの前後関係を維持する。
		if (callbacks.drawImGuiOverlay)
		{
			callbacks.drawImGuiOverlay();
		}
	}

	void RenderPipelineController::ExecuteGameFrame(const FrameCallbacks& callbacks)
	{
		// Game Preview / ReleaseでもSceneRenderTargetをPostEffect入力にし、Debugと描画経路を揃える。
		if (callbacks.drawGameWorldToSceneTarget)
		{
			callbacks.drawGameWorldToSceneTarget();
		}

		// PostEffect後の結果をBackBufferへ出力し、その後のUIを画面効果の対象外にする。
		if (callbacks.applyPostEffectToBackBuffer)
		{
			callbacks.applyPostEffectToBackBuffer();
		}

		// GPU ParticleやPostEffectでRTVが切り替わった後、BackBufferへHUD/UIを重ねるため再バインドする。
		if (callbacks.rebindBackBufferForGameOverlay)
		{
			callbacks.rebindBackBufferForGameOverlay();
		}

		if (callbacks.drawGameUIToBackBuffer)
		{
			callbacks.drawGameUIToBackBuffer();
		}
	}
}
