#include "RandomEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>	
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>
#include <WinApp.h>

#include <cassert>
#include <imgui.h>


/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void RandomEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"RandomEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(RandomSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&randomSetting_));

	// ランダムエフェクトの設定
	randomSetting_->time = 0.0f; // 時間
	randomSetting_->useMultiply = false; // 乗算を使用するかどうか
}


/// -------------------------------------------------------------
///						　更新処理
/// -------------------------------------------------------------
void RandomEffect::Update()
{
	// 時間の更新
	randomSetting_->time += 1.0f / 120.0f; // 適当な値で更新	
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void RandomEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
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
void RandomEffect::DrawImGui()
{
	if (ImGui::Button(randomSetting_->useMultiply ? "No Multiply" : "Apply Multiply")) {
		randomSetting_->useMultiply = !randomSetting_->useMultiply;
	}
}
