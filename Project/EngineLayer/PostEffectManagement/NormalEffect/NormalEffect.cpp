#include "NormalEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>

#include <cassert>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void NormalEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"NormalEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void NormalEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}
