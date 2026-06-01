#pragma once
#include "PipelineCommon.h"

namespace Ken4lowEngine
{
	class PipelineFactory;
	class DXCCompilerManager;

	class Object3DPipelineSet
	{
	public:
		void Initialize(
			PipelineFactory& factory,
			DXCCompilerManager* dxcManager,
			DXGI_FORMAT rtvFormat,
			DXGI_FORMAT dsvFormat);

		void Finalize();

		const PipelineBundle& GetDefault() const { return defaultPipeline_; }
		const PipelineBundle& GetInstanced() const { return instancedPipeline_; }
		const PipelineBundle& GetShadow() const { return shadowPipeline_; }

	private:
		PipelineBundle defaultPipeline_{};
		PipelineBundle instancedPipeline_{};
		PipelineBundle shadowPipeline_{};
	};
}