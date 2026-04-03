#pragma once
#include "PipelineCommon.h"

namespace Ken4lowEngine
{
	class PipelineFactory;
	class DXCCompilerManager;

	class SpritePipelineSet
	{
	public:
		void Initialize(
			PipelineFactory& factory,
			DXCCompilerManager* dxcManager,
			DXGI_FORMAT rtvFormat,
			DXGI_FORMAT dsvFormat);

		const PipelineBundle& GetBackground() const { return backgroundPipeline_; }
		const PipelineBundle& GetUI() const { return uiPipeline_; }

	private:
		PipelineBundle backgroundPipeline_{};
		PipelineBundle uiPipeline_{};
	};
}