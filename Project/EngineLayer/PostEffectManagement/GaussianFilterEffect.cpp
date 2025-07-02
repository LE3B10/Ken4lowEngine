#include "GaussianFilterEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void GaussianFilterEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"GaussianFilterEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GaussianFilterSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&gaussianFilterSetting_));

	// ガウシアンフィルタの設定
	gaussianFilterSetting_->kernelType = 1; // カーネルサイズ
	gaussianFilterSetting_->intensity = 1.0f; // 強度
	gaussianFilterSetting_->threshold = 0.0f; // 閾値
	gaussianFilterSetting_->sigma = 1.0f; // ガウス関数の標準偏差
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void GaussianFilterEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
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
void GaussianFilterEffect::DrawImGui()
{
	const char* kernelOptions[] =
	{
		"None",          // 0
		"Gaussian 3x3",  // 1
		"Gaussian 5x5",  // 2
		"Gaussian 7x7",  // 3
		"Gaussian 9x9"   // 4
	};

	ImGui::Combo("Kernel Type", &gaussianFilterSetting_->kernelType, kernelOptions, IM_ARRAYSIZE(kernelOptions));
	ImGui::SliderFloat("Intensity", &gaussianFilterSetting_->intensity, 0.0f, 5.0f);
	ImGui::SliderFloat("Sigma", &gaussianFilterSetting_->sigma, 0.1f, 5.0f);
}
