#include "RenderPipelineController.h"

#include "DirectXCommon.h"
#include "LightManager.h"

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
		if (callbacks.prepareShadowPass)
		{
			callbacks.prepareShadowPass(); // Editorで変更したLightComponentをCaster選択と行列生成より先に同期する。
		}

		// 通常描画より前という順序は維持し、選択ライトに応じた1/4/6回のSlice描画だけShadowSystemへ委譲する。
		LightManager::GetInstance()->ExecuteShadowPasses(callbacks.drawShadowObjects);
	}

	void RenderPipelineController::ExecuteEditorFrame(const FrameCallbacks& callbacks)
	{
		// Editor UIは既存通り先にImGuiコマンドを組み、最後にBackBufferへOverlayとして描画する。
		if (callbacks.buildEditorUi)
		{
			callbacks.buildEditorUi();
		}

		if (callbacks.executeEditorPickingPass)
		{
			callbacks.executeEditorPickingPass(); // ImGuiで予約したクリックを通常Scene描画前の専用R32_UINT Passで解決する。
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

		if (callbacks.renderEditorSelectionOutline)
		{
			callbacks.renderEditorSelectionOutline(); // Selection Maskと輪郭TextureはGame描画を変更せずImGui合成用に生成する。
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
