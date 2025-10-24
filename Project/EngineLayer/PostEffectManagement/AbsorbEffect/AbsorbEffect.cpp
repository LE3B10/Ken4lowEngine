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
	// DirectX共通クラスのセット
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderCompiler::GetShaderPath(L"AbsorbEffect", L".PS.hlsl"), // ピクセルシェーダーのパス
		rootSignature_.Get(),										 // ルートシグネチャ
		false);														 // 深度なし

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
	absorbSetting_->time += absorbParams_.timestepPerFrame;

	// 時間のリセット
	if (absorbSetting_->time > absorbParams_.loopDuration)
	{
		absorbSetting_->time = 0.0f; // リセット
	}
}


/// -------------------------------------------------------------
///						　適用処理
/// -------------------------------------------------------------
void AbsorbEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	(void)uavIndex; // 未使用
	(void)dsvIndex; // 未使用

	// 描画コマンド
	commandList->SetGraphicsRootSignature(rootSignature_.Get()); // ルートシグネチャの設定
	commandList->SetPipelineState(graphicsPipelineState_.Get()); // パイプラインステートの設定
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex)); // SRVの設定
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());					 // 定数バッファの設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);									 // プリミティブトポロジーの設定
	commandList->DrawInstanced(3, 1, 0, 0);																		 // フルスクリーンクアッドを描画
}


/// -------------------------------------------------------------
///						　ImGui描画処理
/// -------------------------------------------------------------
void AbsorbEffect::DrawImGui()
{
	// 吸収の強さ
	ImGui::SliderFloat("Absorb Strength", &absorbSetting_->strength, 0.0f, 5.0f);
}
