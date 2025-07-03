#include "RadialBlurEffect.h"
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
void RadialBlurEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;
	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();
	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"RadialBlurEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);
	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RadialBlurSetting));
	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurSetting_));

	// ラジアルブラーの設定
	radialBlurSetting_->center = Vector2(0.5f, 0.5f); // 中心座標
	radialBlurSetting_->blurStrength = 1.0f; // ブラー強度
	radialBlurSetting_->sampleCount = 16.0f; // サンプル数
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void RadialBlurEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
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
void RadialBlurEffect::DrawImGui()
{
	ImGui::SliderFloat("Blur Strength", &radialBlurSetting_->blurStrength, 0.0f, 5.0f);
	ImGui::SliderFloat("Sample Count", &radialBlurSetting_->sampleCount, 1.0f, 64.0f);
	ImGui::SliderFloat2("Center", &radialBlurSetting_->center.x, 0.0f, 1.0f);
}
