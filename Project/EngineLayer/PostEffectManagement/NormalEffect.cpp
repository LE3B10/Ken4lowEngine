#include "NormalEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <SRVManager.h>
#include <ShaderManager.h>

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
		ShaderManager::GetShaderPath(L"NormalEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void NormalEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
{
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex));

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}
