#define NOMINMAX
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

		Vector3 TransformHomogeneousPoint(const Matrix4x4& m, const Vector3& p)
		{
			const float x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
			const float y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
			const float z = p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2];
			const float w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
			if (std::fabs(w) < 1e-6f) { return { x, y, z }; }
			return { x / w, y / w, z / w };
		}

		void DrawFrustumWireframeFromLightVP(Wireframe* wf, const Matrix4x4& lightViewProjection, const Vector4& color)
		{
			Matrix4x4 inv = Matrix4x4::Inverse(lightViewProjection);
			const Vector3 ndcCorners[8] = {
				{-1.0f,-1.0f,0.0f},{1.0f,-1.0f,0.0f},{1.0f,1.0f,0.0f},{-1.0f,1.0f,0.0f},
				{-1.0f,-1.0f,1.0f},{1.0f,-1.0f,1.0f},{1.0f,1.0f,1.0f},{-1.0f,1.0f,1.0f}
			};
			Vector3 wpos[8];
			for (int i = 0; i < 8; ++i) { wpos[i] = TransformHomogeneousPoint(inv, ndcCorners[i]); }
			const int e[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
			for (auto& edge : e) { wf->DrawLine(wpos[edge[0]], wpos[edge[1]], color); }
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
			if (L.lightType == 0 || L.enabled == 0u) continue;           // 無効はスキップ
			PunctualLightGPU C = L;
			if (C.lightType == 1 || C.lightType == 3 || C.lightType == 4 || C.lightType == 5) // Dir/Spot/Area は方向を正規化
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
			if (L.enabled == 0u) { continue; }
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
				DrawFrustumWireframeFromLightVP(wf, currentShadowLightViewProjection_, { 0.1f, 0.9f, 1.0f, 1.0f });
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
			case 4: { // RectArea (approx)
				const Vector4 colArea = { 0.4f, 1.0f, 0.4f, 1.0f };
				Vector3 d = Vector3::Normalize(L.direction);
				Vector3 up = (std::fabs(d.y) > 0.95f) ? Vector3{ 1.0f,0.0f,0.0f } : Vector3{ 0.0f,1.0f,0.0f };
				Vector3 right = Vector3::Normalize(Vector3::Cross(up, d));
				up = Vector3::Normalize(Vector3::Cross(d, right));
				float hw = std::max(L.areaSize.x * 0.5f, 0.05f);
				float hh = std::max(L.areaSize.y * 0.5f, 0.05f);
				Vector3 c = L.position;
				Vector3 p0 = c + right * hw + up * hh;
				Vector3 p1 = c - right * hw + up * hh;
				Vector3 p2 = c - right * hw - up * hh;
				Vector3 p3 = c + right * hw - up * hh;
				wf->DrawLine(p0, p1, colArea); wf->DrawLine(p1, p2, colArea);
				wf->DrawLine(p2, p3, colArea); wf->DrawLine(p3, p0, colArea);
				wf->DrawLine(c, c + d * dirLen, colArea);
				break;
			}
			case 5: { // SphereArea (approx)
				const Vector4 colAreaSphere = { 0.6f, 1.0f, 0.4f, 1.0f };
				wf->DrawSphere(L.position, std::max(L.areaSize.z, 0.1f), colAreaSphere);
				if (L.distance > 0.0f) { wf->DrawSphere(L.position, L.distance, { colAreaSphere.x,colAreaSphere.y,colAreaSphere.z,0.45f }); }
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
			const char* summaryTypes[] = { "None", "Directional", "Point", "Spot", "RectArea", "SphereArea" };
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

		if (ImGui::Button("+ Add Light"))
		{
			PunctualLightGPU L{};
			L.lightType = 1;
			L.color = { 1,1,1,1 };
			L.intensity = 1.0f;
			L.direction = { 0,-1,0 };
			L.areaSize = { 2.0f, 2.0f, 1.0f };
			L.distance = 10.0f;
			L.decay = 1.0f;
			L.enabled = 1u;
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
			const char* types[] = { "None","Directional","Point","Spot","RectArea","SphereArea" };
			if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types)))
			{
				L.lightType = static_cast<uint32_t>(type);
			}

			ImGui::ColorEdit4("Color", &L.color.x);
			ImGui::SliderFloat("Intensity", &L.intensity, 0.0f, 20.0f);
			bool enabled = (L.enabled != 0u);
			if (ImGui::Checkbox("Enabled", &enabled)) { L.enabled = enabled ? 1u : 0u; }

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
			else if (L.lightType == 4)
			{
				// 疑似AreaLightは矩形面の最近点を仮想PointLightとして扱う近似モデル。
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				Vector3 eulerDeg = DirectionToEulerDeg(L.direction);
				bool changed = false;
				changed |= ImGui::DragFloat("Pitch (X)", &eulerDeg.x, 0.5f, -89.0f, 89.0f, "%.1f deg");
				changed |= ImGui::DragFloat("Yaw (Y)", &eulerDeg.y, 0.5f, -180.0f, 180.0f, "%.1f deg");
				if (changed) { L.direction = EulerDegToDirection(eulerDeg); }
				ImGui::SliderFloat("Width", &L.areaSize.x, 0.1f, 50.0f);
				ImGui::SliderFloat("Height", &L.areaSize.y, 0.1f, 50.0f);
				ImGui::SliderFloat("Range", &L.distance, 0.1f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
			}
			else if (L.lightType == 5)
			{
				ImGui::SliderFloat3("Position", &L.position.x, -50.0f, 50.0f);
				if (ImGui::SliderFloat3("Direction", &L.direction.x, -1.0f, 1.0f)) { L.direction = Vector3::Normalize(L.direction); }
				ImGui::SliderFloat("Radius", &L.areaSize.z, 0.1f, 50.0f);
				ImGui::SliderFloat("Range", &L.distance, 0.1f, 200.0f);
				ImGui::SliderFloat("Decay", &L.decay, 0.0f, 10.0f);
			}
			ImGui::Text("AreaLight active: %s", (L.enabled && (L.lightType == 4 || L.lightType == 5)) ? "true" : "false");
			ImGui::Text("AreaLight type: %s", (L.lightType == 4) ? "RectArea" : ((L.lightType == 5) ? "SphereArea" : "N/A"));
			ImGui::Text("area size: (%.2f, %.2f, %.2f)", L.areaSize.x, L.areaSize.y, L.areaSize.z);
			ImGui::Text("range: %.2f  intensity: %.2f", L.distance, L.intensity);
			ImGui::Text("light direction: (%.2f, %.2f, %.2f)", L.direction.x, L.direction.y, L.direction.z);
			ImGui::Text("debug wire visible: %s", ((L.enabled != 0u) && (L.lightType == 4 || L.lightType == 5)) ? "true" : "false");

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
		const bool hasPointLight = std::any_of(punctualLights_.begin(), punctualLights_.end(), [](const PunctualLightGPU& light) { return light.lightType == 2 && light.intensity > 0.0f && light.enabled != 0u; });
		const bool hasAreaLight = std::any_of(punctualLights_.begin(), punctualLights_.end(), [](const PunctualLightGPU& light) { return (light.lightType == 4 || light.lightType == 5) && light.intensity > 0.0f && light.enabled != 0u; });
		const bool hasShadowCaster = (GetActiveShadowCasterType() != ShadowCasterType::None);
		if (!hasShadowCaster && hasPointLight)
		{
			ImGui::BeginDisabled();
			ImGui::Checkbox("Enable Shadow", &enableShadow_);
			ImGui::EndDisabled();
		}
		else
		{
			ImGui::Checkbox("Enable Shadow", &enableShadow_);
		}
		ImGui::SliderFloat("Shadow Bias", &shadowBias_, 0.0f, 0.01f, "%.6f");
		ImGui::SliderFloat("Normal Bias", &normalBias_, 0.0f, 0.1f, "%.4f");
		ImGui::SliderFloat("Shadow Strength", &shadowStrength_, 0.0f, 1.0f);
		int shadowMapSize = static_cast<int>(shadowMapSize_);
		if (ImGui::InputInt("Shadow Map Size", &shadowMapSize)) {
			shadowMapSize_ = static_cast<uint32_t>(std::clamp(shadowMapSize, 256, 4096));
			dxCommon_->SetShadowMapSize(shadowMapSize_, shadowMapSize_);
		}
		ImGui::Checkbox("Show ShadowMap Debug", &showShadowMapDebug_);
		ImGui::Checkbox("Show Shadow Factor", &showShadowFactorDebug_);

		// ステージに合わせて影の有効範囲を調整できるよう、固定値ではなく設定値を使用する。
		ImGui::SeparatorText("Shadow Frustum");
		ImGui::SliderFloat("Directional Shadow Distance", &directionalShadowDistance_, 1.0f, 500.0f, "%.2f");
		ImGui::SliderFloat("Directional Shadow Width", &directionalShadowWidth_, 5.0f, 300.0f, "%.2f");
		ImGui::SliderFloat("Directional Shadow Height", &directionalShadowHeight_, 5.0f, 300.0f, "%.2f");
		ImGui::SliderFloat("Directional Shadow NearZ", &directionalShadowNearZ_, 0.01f, 50.0f, "%.3f");
		ImGui::SliderFloat("Directional Shadow FarZ", &directionalShadowFarZ_, 1.01f, 1000.0f, "%.2f");
		const char* focusModeItems[] = { "Camera", "Player", "StageCenter", "Manual" };
		int focusMode = static_cast<int>(shadowFocusMode_);
		if (ImGui::Combo("Shadow Focus Mode", &focusMode, focusModeItems, IM_ARRAYSIZE(focusModeItems))) { shadowFocusMode_ = static_cast<ShadowFocusMode>(focusMode); }
		ImGui::DragFloat3("Manual Shadow Focus Position", &manualShadowFocusPosition_.x, 0.1f);
		ImGui::SliderFloat("Shadow Focus Offset", &directionalShadowFocusOffset_, -200.0f, 200.0f, "%.2f");
		ImGui::SliderFloat("Spot Shadow NearZ", &spotShadowNearZ_, 0.01f, 50.0f, "%.3f");

		directionalShadowDistance_ = std::clamp(directionalShadowDistance_, 1.0f, 500.0f);
		directionalShadowWidth_ = std::clamp(directionalShadowWidth_, 5.0f, 300.0f);
		directionalShadowHeight_ = std::clamp(directionalShadowHeight_, 5.0f, 300.0f);
		directionalShadowNearZ_ = std::clamp(directionalShadowNearZ_, 0.01f, 50.0f);
		directionalShadowFarZ_ = std::clamp(directionalShadowFarZ_, directionalShadowNearZ_ + 1.0f, 1000.0f);
		spotShadowNearZ_ = std::clamp(spotShadowNearZ_, 0.01f, 50.0f);
				if (hasPointLight)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "PointLight Shadow: Not Implemented (Cube ShadowMap required)");
			ImGui::Text("Enable Shadow affects Directional/Spot only.");
		}
		else
		{
			ImGui::Text("PointLight Shadow: Not Implemented");
		}
		if (hasAreaLight)
		{
			ImGui::TextColored(ImVec4(0.75f, 1.0f, 0.75f, 1.0f), "AreaLight Shadow: Not Implemented");
			ImGui::Text("AreaLight Model: Approximation");
		}
		for (const auto& L : punctualLights_)
		{
			if (L.lightType == 3)
			{
				ImGui::Text("Spot cone cosOuter/cosInner: %.3f / %.3f", L.cosAngle, L.cosFalloffStart);
				ImGui::Text("Spot range: %.2f", L.distance);
				break;
			}
		}
		const ShadowCasterType casterType = GetActiveShadowCasterType();
		if (casterType == ShadowCasterType::Directional)
		{
			ImGui::Text("Directional: LightViewProjection active");
		}
		else if (casterType == ShadowCasterType::Spot)
		{
			ImGui::Text("Spot: LightViewProjection active");
			ImGui::Text("LightViewProjection: generated in LightManager (spot)");
		}
		else if (hasPointLight)
		{
			ImGui::Text("Point: Not used, Cube ShadowMap required");
		}
		else
		{
			ImGui::Text("None: no shadow-casting light selected");
		}
		const char* activeCasterName = (casterType == ShadowCasterType::Directional) ? "Directional" : (casterType == ShadowCasterType::Spot) ? "Spot" : "None";
		ImGui::SeparatorText("Shadow Debug");
		ImGui::Text("Active Shadow Caster: %s", activeCasterName);
		ImGui::Text("Shadow Focus Position: (%.2f, %.2f, %.2f)", currentShadowFocusPosition_.x, currentShadowFocusPosition_.y, currentShadowFocusPosition_.z);
		ImGui::Text("Shadow Direction: (%.3f, %.3f, %.3f)", currentShadowDirection_.x, currentShadowDirection_.y, currentShadowDirection_.z);
		ImGui::Text("Shadow Distance: %.2f", directionalShadowDistance_);
		ImGui::Text("Shadow Width / Height: %.2f / %.2f", directionalShadowWidth_, directionalShadowHeight_);
		ImGui::Text("Shadow Near / Far: %.3f / %.2f", directionalShadowNearZ_, directionalShadowFarZ_);
		ImGui::Text("Shadow Map Size: %u", shadowMapSize_);
		ImGui::Text("Shadow Bias / Normal Bias: %.6f / %.4f", shadowBias_, normalBias_);
		ImGui::Text("Active Lights (type!=0): will be uploaded");
#endif // USE_IMGUI
	}


	LightManager::ShadowCasterType LightManager::GetActiveShadowCasterType() const
	{
		for (const auto& light : punctualLights_)
		{
			if (light.intensity <= 0.0f || light.enabled == 0u) { continue; }
			if (light.lightType == 3) { return ShadowCasterType::Spot; }
			if (light.lightType == 1) { return ShadowCasterType::Directional; }
		}
		return ShadowCasterType::None;
	}

	Matrix4x4 LightManager::BuildShadowLightViewProjection(const Vector3& focusPosition) const
	{
		Vector3 lightDir = { 0.3f, -1.0f, 0.2f };
		for (const auto& light : punctualLights_)
		{
			if (light.lightType == 1 && light.intensity > 0.0f)
			{
				lightDir = Vector3::Normalize(light.direction);
				break;
			}
		}

		if (GetActiveShadowCasterType() == ShadowCasterType::Spot)
		{
			for (const auto& light : punctualLights_)
			{
				if (light.lightType != 3 || light.intensity <= 0.0f) { continue; }
				const float spotDistance = std::max(light.distance, 5.0f);
				const float spotOuterCos = std::clamp(light.cosAngle, 0.01f, 0.999f);
				const float outerAngle = std::acos(spotOuterCos) * 2.0f;
				const float fovY = std::clamp(outerAngle, 0.15f, 3.0f);
				// SpotLightの方向でShadowMap用ViewProjectionを生成する。
				const Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(light.position, light.position + Vector3::Normalize(light.direction), { 0.0f,1.0f,0.0f });
				const Matrix4x4 proj = Matrix4x4::MakePerspectiveFovMatrix(fovY, 1.0f, spotShadowNearZ_, spotDistance);
				// SpotLight Shadow は world * view * projection の順で評価できる行列を返す。
				return Matrix4x4::Multiply(view, proj);
			}
		}

		Vector3 directionalFocusPosition = focusPosition;
		if (shadowFocusMode_ == ShadowFocusMode::Manual || shadowFocusMode_ == ShadowFocusMode::StageCenter)
		{
			// Shadow Frustumの中心を調整できるようにして、ステージ全体を影範囲に収めやすくする
			directionalFocusPosition = manualShadowFocusPosition_;
		}
		directionalFocusPosition += Vector3{ 0.0f, directionalShadowFocusOffset_, 0.0f };
		const float shadowNear = std::clamp(directionalShadowNearZ_, 0.01f, 500.0f);
		const float shadowFar = std::max(directionalShadowFarZ_, shadowNear + 1.0f);
		const Matrix4x4 lightViewProjection = Matrix4x4::MakeLightViewProjection(
			lightDir, directionalFocusPosition, directionalShadowDistance_, directionalShadowWidth_, directionalShadowHeight_, shadowNear, shadowFar);
		currentShadowFocusPosition_ = directionalFocusPosition;
		currentShadowDirection_ = lightDir;
		currentShadowLightViewProjection_ = lightViewProjection;
		return lightViewProjection;
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
		light.enabled = 1u;

		punctualLights_.clear();
		punctualLights_.push_back(light);
	}

} // namespace Ken4lowEngine
