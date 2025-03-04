#include "PipelineStateManager.h"
#include "DirectXCommon.h"


/// -------------------------------------------------------------
///					パイプラインを追加する処理
/// -------------------------------------------------------------
void PipelineStateManager::AddPipeline(std::unique_ptr<PipelineState> pipeline)
{
	pipelines_.push_back(std::move(pipeline));
}


/// -------------------------------------------------------------
///				全てのパイプラインを適用する処理
/// -------------------------------------------------------------
void PipelineStateManager::RenderAll()
{
	for (const auto& pipeline : pipelines_)
	{
		pipeline->Render();
	}
}


/// -------------------------------------------------------------
///					パイプラインを取得する処理
/// -------------------------------------------------------------
PipelineState* PipelineStateManager::GetPipeline(size_t index)
{
	if (index < pipelines_.size())
	{
		return pipelines_[index].get();
	}
	return nullptr;
}
