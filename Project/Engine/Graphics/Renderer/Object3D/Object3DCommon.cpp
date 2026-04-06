#include "Object3DCommon.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{
	Object3DCommon* Object3DCommon::GetInstance()
	{
		static Object3DCommon instance;
		return &instance;
	}

	void Object3DCommon::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;

		pipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);

		LightManager::GetInstance()->Initialize(dxCommon_);
	}

	void Object3DCommon::Finalize()
	{
		LightManager::GetInstance()->Finalize();
		pipelineSet_.Finalize();
		dxCommon_ = nullptr;
	}

	void Object3DCommon::DrawImGui()
	{
#ifdef USE_IMGUI
#endif
	}

	void Object3DCommon::SetRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetDefault();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
	}

	void Object3DCommon::SetShadowMapRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetShadow();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}