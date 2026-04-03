#pragma once
#include "PipelineCommon.h"

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class PipelineFactory;
	class DXCCompilerManager;

	/// -------------------------------------------------------------
	///				　スプライト描画用のパイプラインセット
	/// -------------------------------------------------------------
	class SpritePipelineSet
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// パイプラインを初期化します。
		/// </summary>
		/// <param name="factory">パイプラインの作成に使用するファクトリ。</param>
		/// <param name="shaderCompiler">シェーダーのコンパイルに使用するコンパイラ。</param>
		/// <param name="rtvFormat">レンダーターゲットビューのフォーマット。</param>
		/// <param name="dsvFormat">深度ステンシルビューのフォーマット。</param>
		void Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

		void Finalize();

	public: /// ---------- アクセッサ ---------- ///

		const PipelineBundle& GetBackground() const { return backgroundPipeline_; }
		const PipelineBundle& GetUI() const { return uiPipeline_; }

	private: /// ---------- メンバ変数 ---------- ///

		PipelineBundle backgroundPipeline_{};
		PipelineBundle uiPipeline_{};
	};

}