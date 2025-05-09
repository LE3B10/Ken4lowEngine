#include "LuminanceOutlineEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderManager.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void LuminanceOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderManager::GetShaderPath(L"LuminanceOutlineEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LuminanceOutlineSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&luminanceOutlineSetting_));

	// アウトラインの設定
	luminanceOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
	luminanceOutlineSetting_->edgeStrength = 1.0f; // エッジ強度
	luminanceOutlineSetting_->threshold = 0.5f; // 閾値
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void LuminanceOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
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
void LuminanceOutlineEffect::DrawImGui()
{
	ImGui::SliderFloat("Edge Strength", &luminanceOutlineSetting_->edgeStrength, 0.0f, 5.0f);
	ImGui::SliderFloat("Threshold", &luminanceOutlineSetting_->threshold, 0.0f, 1.0f);
}
