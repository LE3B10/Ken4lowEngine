#include "AbsorbEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void AbsorbEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;
	
	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();
	
	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"AbsorbEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);
	
	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(AbsorbSetting));
	
	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&absorbSetting_));

	// 吸収の設定
	absorbSetting_->time = 0.0f; // 時間
	absorbSetting_->strength = 1.0f; // 吸収の強さ
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void AbsorbEffect::Update()
{
	// 時間の更新
	absorbSetting_->time += 1.0f / 12.0f;
	if (absorbSetting_->time > 1.0f) {
		absorbSetting_->time = 0.0f; // リセット
	}
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void AbsorbEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void AbsorbEffect::DrawImGui()
{
	ImGui::SliderFloat("Absorb Strength", &absorbSetting_->strength, 0.0f, 5.0f);
}
