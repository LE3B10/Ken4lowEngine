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

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

		Vector3 DirectionToEulerDeg(const Vector3& dir)
		{
			Vector3 n = Vector3::Normalize(dir);

			// pitch(X): 上下
			float pitch = std::asin(-n.y);

			// yaw(Y): 左右
			float yaw = std::atan2(n.x, n.z);

			// directional light の向き自体には roll は基本不要
			return {
				pitch * kRadToDeg,
				yaw * kRadToDeg,
				0.0f
			};
		}

		Vector3 EulerDegToDirection(const Vector3& eulerDeg)
		{
			float pitch = eulerDeg.x * kDegToRad;
			float yaw = eulerDeg.y * kDegToRad;

			float cp = std::cos(pitch);
			float sp = std::sin(pitch);
			float cy = std::cos(yaw);
			float sy = std::sin(yaw);

			// 左手系寄りの前方向ベース
			Vector3 dir;
			dir.x = sy * cp;
			dir.y = -sp;
			dir.z = cy * cp;

			return Vector3::Normalize(dir);
		}
	}

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

		punctualBuffer_->SetName(L"PunctualLightBuffer");
		lightInfoResource_->SetName(L"LightInfoConstantBuffer");
	}

	void LightManager::Finalize()
	{
		// SRV インデックス返却（二重Free防止のためフラグと無効値を戻す）
		if (punctualSRVAllocated_ && punctualSRVIndex_ != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(punctualSRVIndex_);
			punctualSRVIndex_ = UINT32_MAX;
			punctualSRVAllocated_ = false;
		}

		// Mapしているポインタは、Resourceを落とす前に無効化（Unmapは必須ではないが安全のため）
		if (lightInfoResource_)
		{
			lightInfoResource_->Unmap(0, nullptr);
			lightInfoData_ = nullptr;
		}

		// GPU/COMリソース解放
		punctualBuffer_.Reset();
		punctualBufferBytes_ = 0;
		lightInfoResource_.Reset();

		// CPU側データ
		punctualLights_.clear();
		punctualType_ = 1;

		// 借り物参照
		dxCommon_ = nullptr;
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
		if (!punctualBuffer_)
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
#ifdef _DEBUG

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
				Vector3 base = { 0.0f, 3.0f, 0.0f };
				Vector3 d = Vector3::Normalize(L.direction);

				Vector3 tip = base - d * dirLen;
				wf->DrawLine(base, tip, colDir);

				// 矢印の羽
				Vector3 up = { 0.0f, 1.0f, 0.0f };
				if (std::fabs(Vector3::Dot(d, up)) > 0.95f)
				{
					up = { 1.0f, 0.0f, 0.0f };
				}

				Vector3 right = Vector3::Normalize(Vector3::Cross(up, d));
				Vector3 sideUp = Vector3::Normalize(Vector3::Cross(d, right));

				const float headLen = 0.35f;
				const float headWidth = 0.18f;

				Vector3 headBase = tip + d * headLen;

				wf->DrawLine(tip, headBase + right * headWidth, colDir);
				wf->DrawLine(tip, headBase - right * headWidth, colDir);
				wf->DrawLine(tip, headBase + sideUp * headWidth, colDir);
				wf->DrawLine(tip, headBase - sideUp * headWidth, colDir);

				// 方向が分かりやすいように始点にも小さい十字
				const float crossSize = 0.15f;
				wf->DrawLine(base - right * crossSize, base + right * crossSize, colDir);
				wf->DrawLine(base - sideUp * crossSize, base + sideUp * crossSize, colDir);
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
#endif // _DEBUG
	}


	/// -------------------------------------------------------------
	///				　		　	ImGui
	/// -------------------------------------------------------------
	void LightManager::DrawImGui()
	{
#ifdef USE_IMGUI
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
					Vector3 eulerDeg = DirectionToEulerDeg(L.direction);

					bool changed = false;
					changed |= ImGui::DragFloat("Pitch (X)", &eulerDeg.x, 0.5f, -89.0f, 89.0f, "%.1f deg");
					changed |= ImGui::DragFloat("Yaw (Y)", &eulerDeg.y, 0.5f, -180.0f, 180.0f, "%.1f deg");

					// Roll は光の向き自体には効かないので表示だけにするか、隠す
					ImGui::BeginDisabled();
					ImGui::DragFloat("Roll (Z)", &eulerDeg.z, 0.5f, -180.0f, 180.0f, "%.1f deg");
					ImGui::EndDisabled();

					if (changed)
					{
						L.direction = EulerDegToDirection(eulerDeg);
					}

					ImGui::Text("Dir = (%.3f, %.3f, %.3f)", L.direction.x, L.direction.y, L.direction.z);
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
#endif // USE_IMGUI
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

	void LightManager::AddDefaultDirectionalLight()
	{
		PunctualLightGPU light{};
		light.lightType = 1; // Directional
		light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		light.intensity = 1.0f;
		light.direction = Vector3::Normalize({ 0.3f, -1.0f, 0.2f });

		punctualLights_.clear();
		punctualLights_.push_back(light);
	}

} // namespace Ken4lowEngine
