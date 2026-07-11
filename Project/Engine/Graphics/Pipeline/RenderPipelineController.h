#pragma once

#include <functional>

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// <summary>
	/// 既存描画処理を並べるだけの薄いレンダーパイプライン入口です。<br/>
	/// RenderPass/FrameGraphはまだ導入せず、GameApplicationに散らばっていた1フレーム内の
	/// Shadow/Scene/PostEffect/UI/Present手前までの順序を見える化するためのFacadeとして機能します。
	/// </summary>
	class RenderPipelineController
	{
	public:
		/// <summary>
		/// GameApplicationが既存描画関数を差し込むためのコールバック群です。<br/>
		/// Controllerは描画リソースを所有せず、既存関数の呼び出し順だけを管理します。
		/// </summary>
		struct FrameCallbacks
		{
			std::function<void()> prepareShadowPass;
			std::function<void()> drawShadowObjects;
			std::function<void()> buildEditorUi;
			std::function<void()> drawGameWorldToSceneTarget;
			std::function<void()> renderPostEffectToGameRenderTarget;
			std::function<void()> beginGameRenderTargetOverlay;
			std::function<void()> drawScene2DOverlay;
			std::function<void()> endGameRenderTargetOverlay;
			std::function<void()> drawImGuiOverlay;
			std::function<void()> applyPostEffectToBackBuffer;
			std::function<void()> rebindBackBufferForGameOverlay;
			std::function<void()> drawGameUIToBackBuffer;
		};

		/// <summary>
		/// DirectXCommonを保持し、低レベル描画APIを呼ぶための入口を設定します。<br/>
		/// リソース生成やCommandList所有はDirectXCommon側に残します。
		/// </summary>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// 1フレーム分の既存描画順序を実行します。<br/>
		/// editorModeEnabledがtrueのときはMain Viewport用GameRenderTargetへ描画し、falseのときはBackBufferへ出力します。
		/// </summary>
		void ExecuteFrame(bool editorModeEnabled, const FrameCallbacks& callbacks);

	private:
		/// <summary>
		/// ShadowMapへ深度を書き込む既存パスを実行します。<br/>
		/// 通常3D描画より先に行うことで、後段のライティングがShadowMapを参照できる順序を維持します。
		/// </summary>
		void ExecuteShadowMapPass(const FrameCallbacks& callbacks);

		/// <summary>
		/// Editor Modeの既存描画順を実行します。<br/>
		/// ImGuiのMain Viewportへ表示するため、3D/PostEffect/UIをGameRenderTargetに集約してからImGuiを描画します。
		/// </summary>
		void ExecuteEditorFrame(const FrameCallbacks& callbacks);

		/// <summary>
		/// Game Preview / Release相当の既存描画順を実行します。<br/>
		/// SceneRenderTargetへ3Dを描いた後、PostEffect結果をBackBufferへ出してUIを重ねます。
		/// </summary>
		void ExecuteGameFrame(const FrameCallbacks& callbacks);

		DirectXCommon* dxCommon_ = nullptr;
	};
}