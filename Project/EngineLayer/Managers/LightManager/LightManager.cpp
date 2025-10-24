#include "LightManager.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "ImGuiManager.h"
#include "ParameterManager.h"
#include <SRVManager.h>
#include "Wireframe.h"

#include <numbers>    // 円周率（C++20）
#include <algorithm>  // std::clamp
#include <cmath>      // sin/cos/atan2/asin/acos

LightManager* LightManager::GetInstance()
{
	static LightManager instance;
	return &instance;
}

/// -------------------------------------------------------------
///				　		初期化処理
/// -------------------------------------------------------------
void LightManager::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;

	CreatePunctualLight();
}


/// -------------------------------------------------------------
///				　		パンクチュアルライトの生成
/// -------------------------------------------------------------
void LightManager::CreatePunctualLight()
{
	/// ---------- ライト数CBの生成 ---------- ///
	if (!lightInfoResource_)
	{
		// ライト数CB用のリソースを作る
		lightInfoResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LightInfo));
		lightInfoResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightInfoData_));
		lightInfoData_->lightCount = 0; // ライトの数
	}

	/// ---------- SRVスロットの確保 ---------- ///
	if (!punctualSRVAllocated_)
	{
		punctualSRVIndex_ = SRVManager::GetInstance()->Allocate();
		punctualSRVAllocated_ = true;
	}

	/// ---------- GPUバッファは初期化時点では最小確保 ---------- ///
	if (!punctualSRVAllocated_)
	{
		const uint32_t stride = sizeof(PunctualLightGPU);
		const uint32_t minElems = 1;
		const uint32_t minBytes = stride * minElems;

		punctualBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), minBytes);
		punctualBufferBytes_ = minBytes;

		// NumElements は最低 1
		SRVManager::GetInstance()->CreateSRVForStructureBuffer(punctualSRVIndex_, punctualBuffer_.Get(), minElems, stride);
	}
}


/// -------------------------------------------------------------
///				　		パンクチュアルライトの更新
/// -------------------------------------------------------------
void LightManager::UpdatePunctualLight()
{
	// ===== 有効ライトだけをGPU転送対象に ====
	std::vector<PunctualLightGPU> gpuLights;
	gpuLights.reserve(punctualLights_.size());
	for (const auto& L : punctualLights_) {
		if (L.lightType == 0) continue;           // 無効はスキップ
		PunctualLightGPU C = L;
		if (C.lightType == 1 || C.lightType == 3) // Dir/Spot は方向を正規化して安全に
			C.direction = Vector3::Normalize(C.direction);
		gpuLights.push_back(C);
	}

	// ===== バッファ確保（0本でも最小1要素分を確保） =====
	const uint32_t stride = sizeof(PunctualLightGPU);
	const uint32_t elemCount = static_cast<uint32_t>(gpuLights.size());
	const uint32_t safeCount = (elemCount == 0) ? 1u : elemCount;     // 最低1
	const uint32_t bytes = stride * safeCount;

	if (!punctualBuffer_ || punctualBufferBytes_ < bytes) {
		punctualBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), bytes);
		punctualBufferBytes_ = bytes;
	}

	// データ書き込み（実データがあるときだけ）
	if (elemCount > 0) {
		void* mapped = nullptr;
		punctualBuffer_->Map(0, nullptr, &mapped);
		std::memcpy(mapped, gpuLights.data(), elemCount * stride);
		punctualBuffer_->Unmap(0, nullptr);
	}

	// SRV再構築（NumElements は最低1）
	SRVManager::GetInstance()->CreateSRVForStructureBuffer(punctualSRVIndex_, punctualBuffer_.Get(), safeCount, stride);

	// ライト数CB（有効ライト数）
	if (lightInfoResource_) {
		lightInfoData_->lightCount = elemCount;   // 0..N
	}

	DebugDrawLightGizmos();
}

/// -------------------------------------------------------------
///				　	ライト情報をシェーダーにバインド
/// -------------------------------------------------------------
void LightManager::DebugDrawLightGizmos()
{
	auto* wf = Wireframe::GetInstance();

	// 可視化パラメータ（お好みで）
	const Vector4 colDir = { 0.0f, 1.0f, 1.0f, 1.0f };  // 平行光: シアン
	const Vector4 colPt = { 1.0f, 1.0f, 0.0f, 1.0f };  // 点光源: 黄
	const Vector4 colSpot = { 1.0f, 0.5f, 0.0f, 1.0f };  // スポット: オレンジ
	const float   rGizmo = 0.25f;                     // 球の半径(表示用)
	const float   dirLen = 1.5f;                      // 方向線の長さ

	for (const auto& L : punctualLights_) {
		switch (L.lightType) {
		case 1: { // Directional
			// 原点付近に「光の向き」を示す矢印線
			Vector3 base = { 0.0f, 3.0f, 0.0f };
			Vector3 d = Vector3::Normalize(L.direction);
			wf->DrawLine(base, base - d * dirLen, colDir);
			break;
		}
		case 2: { // Point
			// 位置に小さな球。到達半径も併せて出したいなら2本目で可視化
			wf->DrawSphere(L.position, rGizmo, colPt);
			if (L.radius > 0.0f) {
				wf->DrawSphere(L.position, L.radius, { colPt.x, colPt.y, colPt.z, 0.5f });
			}
			break;
		}
		case 3: { // Spot
			// 位置に球＋方向線
			wf->DrawSphere(L.position, rGizmo, colSpot);
			Vector3 d = Vector3::Normalize(L.direction);
			wf->DrawLine(L.position, L.position + d * dirLen, colSpot);
			// 必要なら開き角をリングで表現する処理も追加可能
			break;
		}
		default:
			break;
		}
	}
}


/// -------------------------------------------------------------
///				　		　	ImGui
/// -------------------------------------------------------------
void LightManager::DrawImGui()
{
	if (ImGui::CollapsingHeader("Punctual Lights"))
	{

		// 追加ボタン
		if (ImGui::Button("+ Add Light"))
		{
			PunctualLightGPU L{};
			L.lightType = 1;                 // 既定: Directional
			L.color = { 1,1,1,1 };
			L.intensity = 1.0f;
			L.direction = { 0,-1,0 };          // 下向き
			punctualLights_.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All"))
		{
			punctualLights_.clear();
		}

		// 一覧
		for (size_t i = 0; i < punctualLights_.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			auto& L = punctualLights_[i];

			ImGui::Separator();
			ImGui::Text("Light #%zu", i);

			// 種類
			int type = static_cast<int>(L.lightType);
			const char* types[] = { "None","Directional","Point","Spot" };
			if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
				L.lightType = static_cast<uint32_t>(type);

			// 共通
			ImGui::ColorEdit4("Color", &L.color.x);
			ImGui::SliderFloat("Intensity", &L.intensity, 0.0f, 20.0f);

			// 種類別
			if (L.lightType == 1)
			{
				// Directional
				if (ImGui::SliderFloat3("Direction", &L.direction.x, -1.0f, 1.0f))
				{
					L.direction = Vector3::Normalize(L.direction);
				}
			}
			else if (L.lightType == 2)
			{
				// Point
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				ImGui::SliderFloat("Radius", &L.radius, 0.0f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
			}
			else if (L.lightType == 3)
			{
				// Spot
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				if (ImGui::SliderFloat3("Direction", &L.direction.x, -1.0f, 1.0f))
				{
					L.direction = Vector3::Normalize(L.direction);
				}
				ImGui::SliderFloat("Distance", &L.distance, 0.0f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
				ImGui::SliderFloat("cosInner", &L.cosFalloffStart, 0.0f, 1.0f);
				ImGui::SliderFloat("cosOuter", &L.cosAngle, 0.0f, 1.0f);
				if (L.cosFalloffStart < L.cosAngle) L.cosFalloffStart = L.cosAngle; // 内>=外
			}

			if (ImGui::Button("Remove"))
			{
				punctualLights_.erase(punctualLights_.begin() + i);
				ImGui::PopID();
				--i; // 次の要素が詰まるのでインデックス調整
				continue;
			}
			ImGui::PopID();
		}

		// 参考表示（GPUへは UpdatePunctualLight で同期）
		ImGui::Text("Active Lights (type!=0): will be uploaded");
	}
}

/// -------------------------------------------------------------
///				　	ライト情報をシェーダーにバインド
/// -------------------------------------------------------------
void LightManager::BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2)
{
	auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

	UpdatePunctualLight();

	// SRVヒープセット（SRVManagerに任せる）
	SRVManager::GetInstance()->PreDraw();

	// ライト数CBの設定（b2）
	commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b2, lightInfoResource_->GetGPUVirtualAddress());

	// パンクチュアルライトSRVの設定（t2）
	SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(rootIndexSRV_t2, punctualSRVIndex_);
}
