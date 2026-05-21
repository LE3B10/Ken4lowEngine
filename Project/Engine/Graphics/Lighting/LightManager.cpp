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
		if (lightingSettingsResource_)
		{
			lightingSettingsResource_->SetName(L"LightingSettingsConstantBuffer");
		}
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

		if (lightingSettingsResource_)
		{
			lightingSettingsResource_->Unmap(0, nullptr);
			lightingSettingsData_ = nullptr;
		}

		// GPU/COMリソース解放
		punctualBuffer_.Reset();
		punctualBufferBytes_ = 0;
		lightInfoResource_.Reset();
		lightingSettingsResource_.Reset();
		lightingSettings_ = LightingSettingsGPU{};

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

		if (!lightingSettingsResource_)
		{
			// Ambient/ExposureをCPUから段階的に調整できるようにする。
			lightingSettingsResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(LightingSettingsGPU));
			lightingSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightingSettingsData_));
			*lightingSettingsData_ = lightingSettings_;
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
	void LightManager::DrawPunctualLightsInspector()
	{
#ifdef USE_IMGUI
		// Detailsと専用Light Editorの内容差分をなくすため、Punctual Lights本体の描画をここへ集約する。
		ImGui::Text("Light Count: %zu", punctualLights_.size());
		if (!punctualLights_.empty())
		{
			const auto& first = punctualLights_.front();
			const char* summaryTypes[] = { "None", "Directional", "Point", "Spot" };
			const uint32_t typeIndex = (first.lightType < static_cast<uint32_t>(IM_ARRAYSIZE(summaryTypes))) ? first.lightType : 0;
			Vector3 eulerDeg = DirectionToEulerDeg(first.direction);
			ImGui::Text("Light #0 Type: %s", summaryTypes[typeIndex]);
			ImGui::Text("Light #0 Color: (%.3f, %.3f, %.3f, %.3f)", first.color.x, first.color.y, first.color.z, first.color.w);
			ImGui::Text("Light #0 Intensity: %.3f", first.intensity);
			ImGui::Text("Light #0 Pitch / Yaw / Roll: %.1f / %.1f / %.1f", eulerDeg.x, eulerDeg.y, eulerDeg.z);
			ImGui::Text("Light #0 Direction: (%.3f, %.3f, %.3f)", first.direction.x, first.direction.y, first.direction.z);
		}
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Stage Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// 白っぽさの原因切り分け用にAmbient/露出/コントラストを即時調整可能にする。
			ImGui::ColorEdit3("Ambient Color", &lightingSettings_.ambientColor.x);
			ImGui::SliderFloat("Ambient Strength", &lightingSettings_.ambientColor.w, 0.0f, 1.0f);
			ImGui::SliderFloat("Exposure", &lightingSettings_.exposure, 0.25f, 2.0f);
			ImGui::SliderFloat("Contrast", &lightingSettings_.contrast, 0.50f, 1.75f);
			ImGui::SliderFloat("Specular Strength", &lightingSettings_.specularStrength, 0.0f, 0.5f);
			bool enableFog = lightingSettings_.enableFog != 0;
			if (ImGui::Checkbox("Enable Fog", &enableFog))
			{
				lightingSettings_.enableFog = enableFog ? 1u : 0u;
			}
			ImGui::ColorEdit3("Fog Color", &lightingSettings_.fogColor.x);
			ImGui::SliderFloat("Fog Start", &lightingSettings_.fogStart, 0.0f, 250.0f);
			ImGui::SliderFloat("Fog End", &lightingSettings_.fogEnd, lightingSettings_.fogStart + 1.0f, 500.0f);
		}

		
		if (ImGui::CollapsingHeader("Light Debug", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// ライティングの残留成分を切り分けるため各要素を個別に有効/無効化できるようにする。
			bool showLightingDebug = true;
			ImGui::Checkbox("Show Lighting Debug", &showLightingDebug);
			bool ambientOn = lightingSettings_.debugEnableAmbient != 0;
			if (ImGui::Checkbox("Ambient ON/OFF", &ambientOn)) lightingSettings_.debugEnableAmbient = ambientOn ? 1u : 0u;
			bool directionalOn = lightingSettings_.debugEnableDirectional != 0;
			if (ImGui::Checkbox("Directional ON/OFF", &directionalOn)) lightingSettings_.debugEnableDirectional = directionalOn ? 1u : 0u;
			bool pointOn = lightingSettings_.debugEnablePoint != 0;
			if (ImGui::Checkbox("Point ON/OFF", &pointOn)) lightingSettings_.debugEnablePoint = pointOn ? 1u : 0u;
			bool spotOn = lightingSettings_.debugEnableSpot != 0;
			if (ImGui::Checkbox("Spot ON/OFF", &spotOn)) lightingSettings_.debugEnableSpot = spotOn ? 1u : 0u;
			bool fogOn = lightingSettings_.enableFog != 0;
			if (ImGui::Checkbox("Fog ON/OFF", &fogOn)) lightingSettings_.enableFog = fogOn ? 1u : 0u;
			bool emissiveOn = lightingSettings_.debugEnableEmissive != 0;
			if (ImGui::Checkbox("Emissive ON/OFF", &emissiveOn)) lightingSettings_.debugEnableEmissive = emissiveOn ? 1u : 0u;
			bool showShadowFactor = lightingSettings_.debugShowShadowFactor != 0;
			if (ImGui::Checkbox("Show Shadow Factor", &showShadowFactor)) lightingSettings_.debugShowShadowFactor = showShadowFactor ? 1u : 0u;
			bool showContribution = lightingSettings_.debugShowLightContribution != 0;
			if (ImGui::Checkbox("Show Light Contribution", &showContribution)) lightingSettings_.debugShowLightContribution = showContribution ? 1u : 0u;

			int directionalCount = 0, pointCount = 0, spotCount = 0;
			for (const auto& l : punctualLights_) {
				if (l.lightType == 1) ++directionalCount;
				else if (l.lightType == 2) ++pointCount;
				else if (l.lightType == 3) ++spotCount;
			}
			const float ambientScalar = ((lightingSettings_.debugEnableAmbient != 0) ? lightingSettings_.ambientColor.a : 0.0f);
			ImGui::Text("Ambient      : %.3f", ambientScalar);
			ImGui::Text("Directional  : %d active", directionalCount);
			ImGui::Text("Point        : %d active", pointCount);
			ImGui::Text("Spot         : %d active", spotCount);
			ImGui::Text("Emissive     : %s (material emissive input is not implemented)", (lightingSettings_.debugEnableEmissive != 0) ? "ON" : "OFF");
			ImGui::Text("Fog          : %s", (lightingSettings_.enableFog != 0) ? "ON" : "OFF");
			ImGui::Text("Exposure     : %.3f", lightingSettings_.exposure);
			ImGui::Text("PointLight Shadow Status: Not Implemented");
			ImGui::Text("SpotLight ShadowMap Debug View: Not Implemented");
		}
if (ImGui::Button("+ Add Light"))
		{
			PunctualLightGPU L{};
			L.lightType = 1;
			L.color = { 1,1,1,1 };
			L.intensity = 1.0f;
			L.direction = { 0,-1,0 };
			punctualLights_.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All"))
		{
			punctualLights_.clear();
		}

		for (size_t i = 0; i < punctualLights_.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			auto& L = punctualLights_[i];

			ImGui::Separator();
			ImGui::Text("Light #%zu", i);

			int type = static_cast<int>(L.lightType);
			const char* types[] = { "None","Directional","Point","Spot" };
			if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
			{
				L.lightType = static_cast<uint32_t>(type);
			}

			ImGui::ColorEdit4("Color", &L.color.x);
			ImGui::SliderFloat("Intensity", &L.intensity, 0.0f, 20.0f);

			if (L.lightType == 1)
			{
				Vector3 eulerDeg = DirectionToEulerDeg(L.direction);

				bool changed = false;
				changed |= ImGui::DragFloat("Pitch (X)", &eulerDeg.x, 0.5f, -89.0f, 89.0f, "%.1f deg");
				changed |= ImGui::DragFloat("Yaw (Y)", &eulerDeg.y, 0.5f, -180.0f, 180.0f, "%.1f deg");

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
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				ImGui::SliderFloat("Radius", &L.radius, 0.0f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
			}
			else if (L.lightType == 3)
			{
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				if (ImGui::SliderFloat3("Direction", &L.direction.x, -1.0f, 1.0f))
				{
					L.direction = Vector3::Normalize(L.direction);
				}
				ImGui::SliderFloat("Distance", &L.distance, 0.0f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
				ImGui::SliderFloat("cosInner", &L.cosFalloffStart, 0.0f, 1.0f);
				ImGui::SliderFloat("cosOuter", &L.cosAngle, 0.0f, 1.0f);
				if (L.cosFalloffStart < L.cosAngle) L.cosFalloffStart = L.cosAngle;
			}

			if (ImGui::Button("Remove"))
			{
				punctualLights_.erase(punctualLights_.begin() + i);
				ImGui::PopID();
				--i;
				continue;
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		ImGui::Checkbox("Enable Shadow", &enableShadow_);
		ImGui::SliderFloat("Shadow Bias", &shadowBias_, 0.0f, 0.01f, "%.6f");
		ImGui::SliderFloat("Normal Bias", &normalBias_, 0.0f, 0.1f, "%.4f");
		ImGui::SliderFloat("Shadow Strength", &shadowStrength_, 0.0f, 1.0f);
		int shadowMapSize = static_cast<int>(shadowMapSize_);
		if (ImGui::InputInt("Shadow Map Size", &shadowMapSize)) {
			shadowMapSize_ = static_cast<uint32_t>(std::clamp(shadowMapSize, 256, 4096));
			dxCommon_->SetShadowMapSize(shadowMapSize_, shadowMapSize_);
		}
		ImGui::Checkbox("Show ShadowMap Debug", &showShadowMapDebug_);
		ImGui::Text("Active Lights (type!=0): will be uploaded");
#endif // USE_IMGUI
	}

	void LightManager::DrawImGui(bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのLight Editor表示フラグが閉じている間はライト編集UIを生成しない
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		// ライト調整UIを暗黙のDebugウィンドウではなくDocking可能な通常ウィンドウとして描画する
		ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Light Editor", pOpen))
		{
			if (ImGui::CollapsingHeader("Punctual Lights"))
			{
				DrawPunctualLightsInspector();
			}
		}
		ImGui::End();
#else
		(void)pOpen;
#endif // USE_IMGUI
	}

	/// -------------------------------------------------------------
	///					ライト情報をシェーダーにバインド
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

	void LightManager::BindLightingSettings(uint32_t rootIndexCB_b5)
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		if (lightingSettingsData_)
		{
			// HLSLのLightingSettingsへ最新のAmbient/Exposure/Contrast/Fogを送る。
			*lightingSettingsData_ = lightingSettings_;
		}

		commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b5, lightingSettingsResource_->GetGPUVirtualAddress());
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
