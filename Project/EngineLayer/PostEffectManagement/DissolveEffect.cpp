#include "DissolveEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>
#include <TextureManager.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void DissolveEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"DissolveEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// ディゾルブの設定
	std::string filePath = "Resources/Mask/Noise.png";
	TextureManager::GetInstance()->LoadTexture(filePath);

	// SRVインデックスを取得（CopySRVせず、既存SRVをそのまま使う）
	dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveSetting_));

	// ディゾルブの設定
	dissolveSetting_->threshold = 0.5f;
	dissolveSetting_->edgeThickness = 0.05f;
	dissolveSetting_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void DissolveEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
{
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex));
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(3, SRVManager::GetInstance()->GetGPUDescriptorHandle(dissolveMaskSrvIndex_));
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void DissolveEffect::DrawImGui()
{
	ImGui::SliderFloat("Dissolve Threshold", &dissolveSetting_->threshold, 0.0f, 1.0f);
	ImGui::SliderFloat("Edge Thickness", &dissolveSetting_->edgeThickness, 0.0f, 1.0f);
	ImGui::ColorEdit4("Edge Color", &dissolveSetting_->edgeColor.x);
}
