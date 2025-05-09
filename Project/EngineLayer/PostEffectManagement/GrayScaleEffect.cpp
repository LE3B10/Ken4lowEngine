#include "GrayScaleEffect.h"
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
void GrayScaleEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderManager::GetShaderPath(L"GrayScaleEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(GrayScaleSetting));
	
	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&grayScaleSetting_));

	// グレイスケールエフェクトの設定
	grayScaleSetting_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 強度
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void GrayScaleEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
{
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex));
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void GrayScaleEffect::DrawImGui()
{
	ImGui::ColorEdit4("GrayScale Color", &grayScaleSetting_->color.x);
	ImGui::Text("GrayScale Effect");
	ImGui::Separator();
	ImGui::Text("Intensity: %f", grayScaleSetting_->color.x);
	ImGui::Separator();
}
