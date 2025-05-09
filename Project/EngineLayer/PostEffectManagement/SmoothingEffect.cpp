#include "SmoothingEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderManager.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void SmoothingEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderManager::GetShaderPath(L"SmoothingEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(SmoothingSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&smoothingSetting_));

	// スムージングの設定
	smoothingSetting_->kernelType = 0; // 0: none, 1: box3x3, ...
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void SmoothingEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
{
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex));
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void SmoothingEffect::DrawImGui()
{
	const char* kernelOptions[] =
	{
		"None",          // 0
		"Box 3x3",       // 1
		"Box 5x5",       // 2
		"Gaussian 5x5",  // 3
		"Box 7x7",       // 4
		"Gaussian 7x7",  // 5
		"Box 9x9",       // 6
		"Gaussian 9x9"   // 7
	};

	ImGui::Combo("Kernel Type", &smoothingSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));

}
