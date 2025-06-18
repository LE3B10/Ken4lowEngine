#include "DepthOutlineEffect.h"
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <SRVManager.h>
#include <ShaderManager.h>
#include <WinApp.h>
#include "Camera.h"

#include <cassert>
#include <imgui.h>

DepthOutlineEffect::DepthOutlineEffect(Camera* camera) : camera_(camera)
{
}

void DepthOutlineEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	rootSignature_ = builder->CreateRootSignature();

	// パイプラインの生成
	graphicsPipelineState_ = builder->CreateGraphicsPipeline(
		ShaderManager::GetShaderPath(L"DepthOutlineEffect", L".PS.hlsl"),
		rootSignature_.Get(),
		false);

	// リソースの生成
	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DepthOutlineSetting));

	// データの設定
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&depthOutlineSetting_));

	// アウトラインの設定
	depthOutlineSetting_->texelSize = Vector2(1.0f / static_cast<float>(WinApp::kClientWidth), 1.0f / static_cast<float>(WinApp::kClientHeight));
	depthOutlineSetting_->depthScale = 1.0f; // 輪郭の閾値
	depthOutlineSetting_->edgeThickness = 2.0f; // ピクセル単位の太さ
	depthOutlineSetting_->edgeColor = { 0.0f,0.0f,0.0f,1.0f }; // 黒色

	// 毎フレーム（カメラ更新後に）
	Matrix4x4 proj = camera_->GetProjectionMatrix();
	depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(proj);
}

void DepthOutlineEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t rtvSrvIndex, uint32_t dsvSrvIndex)
{
	depthOutlineSetting_->projectionInverse = Matrix4x4::Inverse(camera_->GetProjectionMatrix());

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());

	// SRVヒープの設定はPostEffectManager側で済ませておく前提
	commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(rtvSrvIndex));
	commandList->SetGraphicsRootConstantBufferView(1, constantBuffer_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(3, SRVManager::GetInstance()->GetGPUDescriptorHandle(dsvSrvIndex));
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void DepthOutlineEffect::DrawImGui()
{
	ImGui::SliderFloat("Depth Scale", &depthOutlineSetting_->depthScale, 0.0f, 100.0f);
	ImGui::SliderFloat("Thickness", &depthOutlineSetting_->edgeThickness, 1.0f, 10.0f);
	ImGui::ColorEdit4("Edge Color", &depthOutlineSetting_->edgeColor.x);
}
