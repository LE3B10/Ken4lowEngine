#define NOMINMAX
#include "LightManager.h"
#include "DirectXCommon.h"
#include <ResourceManager.h>
#include "ImGuiManager.h"
#include "ParameterManager.h"
#include <SRVManager.h>
#include "Wireframe.h"
#include "JsonDataManager.h"
#include "DataAssetPresets.h"

#include <numbers>    // 円周率（C++20）
#include <algorithm>  // std::clamp
#include <cmath>      // sin/cos/atan2/asin/acos
#include <array>
#include <filesystem>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kLightManagerGroup = "LightManager";
		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

		float ClampFinite(float value, float fallback, float minValue, float maxValue)
		{
			if (!std::isfinite(value))
			{
				return fallback;
			}
			return std::clamp(value, minValue, maxValue);
		}

		Vector3 ClampFiniteVector3(const Vector3& value, const Vector3& fallback, const Vector3& minValue, const Vector3& maxValue)
		{
			return {
				ClampFinite(value.x, fallback.x, minValue.x, maxValue.x),
				ClampFinite(value.y, fallback.y, minValue.y, maxValue.y),
				ClampFinite(value.z, fallback.z, minValue.z, maxValue.z)
			};
		}

		Vector4 SanitizeVector4(const Vector4& value, const Vector4& fallback)
		{
			return {
				std::isfinite(value.x) ? value.x : fallback.x,
				std::isfinite(value.y) ? value.y : fallback.y,
				std::isfinite(value.z) ? value.z : fallback.z,
				std::isfinite(value.w) ? value.w : fallback.w
			};
		}

		template<typename T>
		T GetLightParameterOrDefault(ParameterManager* parameters, const std::string& key, const T& fallback)
		{
			try
			{
				return parameters->GetValue<T>(kLightManagerGroup, key);
			}
			catch (const std::exception&)
			{
				return fallback;
			}
		}

		uint32_t NormalizeShadowMapSize(int32_t value)
		{
			constexpr std::array<int32_t, 4> kSafeSizes = { 512, 1024, 2048, 4096 };
			value = std::clamp(value, kSafeSizes.front(), kSafeSizes.back());
			int32_t bestSize = kSafeSizes.front();
			int32_t bestDistance = std::abs(value - bestSize);
			for (int32_t safeSize : kSafeSizes)
			{
				const int32_t distance = std::abs(value - safeSize);
				if (distance < bestDistance)
				{
					bestSize = safeSize;
					bestDistance = distance;
				}
			}
			return static_cast<uint32_t>(bestSize);
		}

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
			Matrix4x4 inv{};
			if (!Matrix4x4::TryInverse(lightViewProjection, inv))
			{
				return;
			}
			const Vector3 ndcCorners[8] = {
				{ -1.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { -1.0f, 1.0f, 0.0f },
				{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f }
			};
			Vector3 wpos[8];
			for (int i = 0; i < 8; ++i)
			{
				wpos[i] = TransformHomogeneousPoint(inv, ndcCorners[i]);
				if (!std::isfinite(wpos[i].x) || !std::isfinite(wpos[i].y) || !std::isfinite(wpos[i].z))
				{
					return;
				}
			}
			const int e[12][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
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

		if (punctualLights_.empty())
		{
			ResetToDefaultLighting();
		}
		RegisterLightParameters();

		punctualBuffer_->SetName(L"PunctualLightBuffer");
		lightInfoResource_->SetName(L"LightInfoConstantBuffer");
		if (lightingSettingsResource_)
		{
			lightingSettingsResource_->SetName(L"LightingSettingsConstantBuffer");
		}
	}

	void LightManager::Finalize()
	{
		UnregisterLightParameters();

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

		for (const auto& L : punctualLights_)
		{
			if (L.enabled == 0u) { continue; }
			switch (L.lightType) {
			case 1:
				{ // Directional
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
			case 2:
				{ // Point
		   // 位置に小さな球。到達半径も併せて出したいなら2本目で可視化
					wf->DrawSphere(L.position, rGizmo, colPt);
					if (L.radius > 0.0f) {
						wf->DrawSphere(L.position, L.radius, { colPt.x, colPt.y, colPt.z, 0.5f });
					}
					break;
				}
			case 3:
				{ // Spot
		   // 位置に球＋方向線
					wf->DrawSphere(L.position, rGizmo, colSpot);
					Vector3 d = Vector3::Normalize(L.direction);
					wf->DrawLine(L.position, L.position + d * dirLen, colSpot);
					// 必要なら開き角をリングで表現する処理も追加可能
					break;
				}
			case 4:
				{ // RectArea (approx)
					const Vector4 colArea = { 0.4f, 1.0f, 0.4f, 1.0f };
					Vector3 d = Vector3::Normalize(L.direction);
					Vector3 up = (std::fabs(d.y) > 0.95f) ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
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
			case 5:
				{ // SphereArea (approx)
					const Vector4 colAreaSphere = { 0.6f, 1.0f, 0.4f, 1.0f };
					wf->DrawSphere(L.position, std::max(L.areaSize.z, 0.1f), colAreaSphere);
					if (L.distance > 0.0f) { wf->DrawSphere(L.position, L.distance, { colAreaSphere.x, colAreaSphere.y, colAreaSphere.z, 0.45f }); }
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
	void LightManager::RegisterLightParameters()
	{
		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kLightManagerGroup);

		// LightManagerの実データをParameterManagerへ集約し、通常ImGuiとの二重管理を避ける。
		parameters->AddItem(kLightManagerGroup, "ambientColor", lightingSettings_.ambientColor);
		parameters->AddItem(kLightManagerGroup, "fogColor", lightingSettings_.fogColor);
		parameters->AddItem(kLightManagerGroup, "exposure", lightingSettings_.exposure, 0.25f, 2.0f);
		parameters->AddItem(kLightManagerGroup, "contrast", lightingSettings_.contrast, 0.50f, 1.75f);
		parameters->AddItem(kLightManagerGroup, "fogStart", lightingSettings_.fogStart, 0.0f, 250.0f);
		parameters->AddItem(kLightManagerGroup, "fogEnd", lightingSettings_.fogEnd, 1.0f, 500.0f);
		parameters->AddItem(kLightManagerGroup, "enableFog", lightingSettings_.enableFog != 0u);
		parameters->AddItem(kLightManagerGroup, "specularStrength", lightingSettings_.specularStrength, 0.0f, 0.5f);
		parameters->AddItem(kLightManagerGroup, "diffuseStrength", lightingSettings_.diffuseStrength, 0.0f, 3.0f);
		parameters->AddItem(kLightManagerGroup, "specularPowerScale", lightingSettings_.specularPowerScale, 0.05f, 4.0f);
		parameters->AddItem(kLightManagerGroup, "rimLightStrength", lightingSettings_.rimLightStrength, 0.0f, 2.0f);
		parameters->AddItem(kLightManagerGroup, "rimLightPower", lightingSettings_.rimLightPower, 0.1f, 8.0f);
		parameters->AddItem(kLightManagerGroup, "enableRimLight", lightingSettings_.enableRimLight != 0u);
		parameters->AddItem(kLightManagerGroup, "enableHalfLambert", lightingSettings_.enableHalfLambert != 0u);
		parameters->AddItem(kLightManagerGroup, "rimLightColor", lightingSettings_.rimLightColor);
		parameters->AddItem(kLightManagerGroup, "shadingMode", static_cast<int32_t>(lightingSettings_.shadingMode), 0, 2);

		parameters->AddItem(kLightManagerGroup, "enableShadow", enableShadow_);
		parameters->AddItem(kLightManagerGroup, "shadowBias", shadowBias_, 0.0f, 0.01f);
		parameters->AddItem(kLightManagerGroup, "normalBias", normalBias_, 0.0f, 0.1f);
		parameters->AddItem(kLightManagerGroup, "shadowStrength", shadowStrength_, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "shadowMapSize", static_cast<int32_t>(shadowMapSize_), 512, 4096);
		parameters->AddItem(kLightManagerGroup, "showShadowMapDebug", showShadowMapDebug_);
		parameters->AddItem(kLightManagerGroup, "showShadowFactorDebug", showShadowFactorDebug_);
		parameters->AddItem(kLightManagerGroup, "shadowCasterLightIndex", shadowCasterLightIndex_, -1, 31);
		parameters->AddItem(kLightManagerGroup, "shadowFocusMode", static_cast<int32_t>(shadowFocusMode_), 0, 3);
		parameters->AddItem(kLightManagerGroup, "manualShadowFocusPosition", manualShadowFocusPosition_, Vector3{ -500.0f, -500.0f, -500.0f }, Vector3{ 500.0f, 500.0f, 500.0f });
		parameters->AddItem(kLightManagerGroup, "directionalShadowDistance", directionalShadowDistance_, 1.0f, 500.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowWidth", directionalShadowWidth_, 5.0f, 300.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowHeight", directionalShadowHeight_, 5.0f, 300.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowNearZ", directionalShadowNearZ_, 0.01f, 50.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowFarZ", directionalShadowFarZ_, 1.01f, 1000.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowFocusOffset", directionalShadowFocusOffset_, -200.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "spotShadowNearZ", spotShadowNearZ_, 0.01f, 50.0f);

		if (punctualLights_.empty())
		{
			AddDefaultDirectionalLight();
		}
		const auto& light = punctualLights_.front();
		parameters->AddItem(kLightManagerGroup, "light0.lightType", static_cast<int32_t>(light.lightType), 0, 5);
		parameters->AddItem(kLightManagerGroup, "light0.enabled", light.enabled != 0u);
		parameters->AddItem(kLightManagerGroup, "light0.color", light.color);
		parameters->AddItem(kLightManagerGroup, "light0.intensity", light.intensity, 0.0f, 20.0f);
		parameters->AddItem(kLightManagerGroup, "light0.direction", light.direction, Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f, 1.0f, 1.0f });
		parameters->AddItem(kLightManagerGroup, "light0.position", light.position, Vector3{ -50.0f, -50.0f, -50.0f }, Vector3{ 50.0f, 50.0f, 50.0f });
		parameters->AddItem(kLightManagerGroup, "light0.radius", light.radius, 0.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "light0.decay", light.decay, 0.0f, 10.0f);
		parameters->AddItem(kLightManagerGroup, "light0.distance", light.distance, 0.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "light0.cosFalloffStart", light.cosFalloffStart, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "light0.cosAngle", light.cosAngle, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "light0.areaSize", light.areaSize, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 50.0f, 50.0f, 50.0f });

		if (std::filesystem::exists("Resources/ParameterManager/LightManager.json"))
		{
			parameters->LoadFile(kLightManagerGroup);
		}
		parameters->RegisterParameterApplier(kLightManagerGroup, this, [this]() { ApplyLightParameters(); }); // Parametersウィンドウの保存/読み込み/反映を実データへ接続する。
		lightParametersRegistered_ = true;
		ApplyLightParameters();
	}

	void LightManager::ApplyLightParameters()
	{
		auto* parameters = ParameterManager::GetInstance();
		lightingSettings_.ambientColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "ambientColor", lightingSettings_.ambientColor), LightingSettingsGPU{}.ambientColor);
		lightingSettings_.fogColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "fogColor", lightingSettings_.fogColor), LightingSettingsGPU{}.fogColor);
		lightingSettings_.exposure = ClampFinite(GetLightParameterOrDefault(parameters, "exposure", lightingSettings_.exposure), 1.0f, 0.05f, 8.0f);
		lightingSettings_.contrast = ClampFinite(GetLightParameterOrDefault(parameters, "contrast", lightingSettings_.contrast), 1.0f, 0.05f, 4.0f);
		lightingSettings_.fogStart = ClampFinite(GetLightParameterOrDefault(parameters, "fogStart", lightingSettings_.fogStart), 45.0f, 0.0f, 10000.0f);
		lightingSettings_.fogEnd = ClampFinite(GetLightParameterOrDefault(parameters, "fogEnd", lightingSettings_.fogEnd), 140.0f, lightingSettings_.fogStart + 1.0f, 20000.0f);
		lightingSettings_.enableFog = GetLightParameterOrDefault(parameters, "enableFog", lightingSettings_.enableFog != 0u) ? 1u : 0u;
		lightingSettings_.specularStrength = ClampFinite(GetLightParameterOrDefault(parameters, "specularStrength", lightingSettings_.specularStrength), 0.08f, 0.0f, 4.0f);
		lightingSettings_.diffuseStrength = ClampFinite(GetLightParameterOrDefault(parameters, "diffuseStrength", lightingSettings_.diffuseStrength), 1.0f, 0.0f, 8.0f);
		lightingSettings_.specularPowerScale = ClampFinite(GetLightParameterOrDefault(parameters, "specularPowerScale", lightingSettings_.specularPowerScale), 1.0f, 0.01f, 16.0f);
		lightingSettings_.rimLightStrength = ClampFinite(GetLightParameterOrDefault(parameters, "rimLightStrength", lightingSettings_.rimLightStrength), 0.0f, 0.0f, 8.0f);
		lightingSettings_.rimLightPower = ClampFinite(GetLightParameterOrDefault(parameters, "rimLightPower", lightingSettings_.rimLightPower), 2.0f, 0.01f, 32.0f);
		lightingSettings_.enableRimLight = GetLightParameterOrDefault(parameters, "enableRimLight", lightingSettings_.enableRimLight != 0u) ? 1u : 0u;
		lightingSettings_.enableHalfLambert = GetLightParameterOrDefault(parameters, "enableHalfLambert", lightingSettings_.enableHalfLambert != 0u) ? 1u : 0u;
		lightingSettings_.rimLightColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "rimLightColor", lightingSettings_.rimLightColor), LightingSettingsGPU{}.rimLightColor);
		lightingSettings_.shadingMode = static_cast<uint32_t>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "shadingMode", static_cast<int32_t>(lightingSettings_.shadingMode)), 0, 2));

		enableShadow_ = GetLightParameterOrDefault(parameters, "enableShadow", enableShadow_);
		shadowBias_ = ClampFinite(GetLightParameterOrDefault(parameters, "shadowBias", shadowBias_), 0.0f, 0.0f, 0.1f);
		normalBias_ = ClampFinite(GetLightParameterOrDefault(parameters, "normalBias", normalBias_), 0.025f, 0.0f, 1.0f);
		shadowStrength_ = ClampFinite(GetLightParameterOrDefault(parameters, "shadowStrength", shadowStrength_), 0.6f, 0.0f, 1.0f);
		const uint32_t sanitizedShadowMapSize = NormalizeShadowMapSize(GetLightParameterOrDefault<int32_t>(parameters, "shadowMapSize", static_cast<int32_t>(shadowMapSize_)));
		if (dxCommon_ && shadowMapSize_ != sanitizedShadowMapSize)
		{
			// ShadowMapの再生成は安全なサイズへ丸めたうえで、実際に変更がある時だけ行う。
			shadowMapSize_ = sanitizedShadowMapSize;
			dxCommon_->SetShadowMapSize(shadowMapSize_, shadowMapSize_);
		}
		else
		{
			shadowMapSize_ = sanitizedShadowMapSize;
		}
		showShadowMapDebug_ = GetLightParameterOrDefault(parameters, "showShadowMapDebug", showShadowMapDebug_);
		showShadowFactorDebug_ = GetLightParameterOrDefault(parameters, "showShadowFactorDebug", showShadowFactorDebug_);
		shadowCasterLightIndex_ = GetLightParameterOrDefault(parameters, "shadowCasterLightIndex", shadowCasterLightIndex_);
		shadowFocusMode_ = static_cast<ShadowFocusMode>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "shadowFocusMode", static_cast<int32_t>(shadowFocusMode_)), 0, 3));
		manualShadowFocusPosition_ = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "manualShadowFocusPosition", manualShadowFocusPosition_), { 0.0f, 0.0f, 0.0f }, { -10000.0f, -10000.0f, -10000.0f }, { 10000.0f, 10000.0f, 10000.0f });
		directionalShadowDistance_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowDistance", directionalShadowDistance_), 60.0f, 1.0f, 5000.0f);
		directionalShadowWidth_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowWidth", directionalShadowWidth_), 35.0f, 1.0f, 5000.0f);
		directionalShadowHeight_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowHeight", directionalShadowHeight_), 35.0f, 1.0f, 5000.0f);
		directionalShadowNearZ_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowNearZ", directionalShadowNearZ_), 0.1f, 0.001f, 5000.0f);
		directionalShadowFarZ_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowFarZ", directionalShadowFarZ_), 120.0f, directionalShadowNearZ_ + 0.001f, 20000.0f);
		directionalShadowFocusOffset_ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowFocusOffset", directionalShadowFocusOffset_), 0.0f, -10000.0f, 10000.0f);
		spotShadowNearZ_ = ClampFinite(GetLightParameterOrDefault(parameters, "spotShadowNearZ", spotShadowNearZ_), 0.1f, 0.001f, 5000.0f);

		if (punctualLights_.empty())
		{
			AddDefaultDirectionalLight();
		}
		auto& light = punctualLights_.front();
		light.lightType = static_cast<uint32_t>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "light0.lightType", static_cast<int32_t>(light.lightType)), 0, 5));
		light.enabled = GetLightParameterOrDefault(parameters, "light0.enabled", light.enabled != 0u) ? 1u : 0u;
		light.color = SanitizeVector4(GetLightParameterOrDefault(parameters, "light0.color", light.color), { 1.0f, 1.0f, 1.0f, 1.0f });
		light.intensity = ClampFinite(GetLightParameterOrDefault(parameters, "light0.intensity", light.intensity), 1.0f, 0.0f, 100.0f);
		light.direction = Vector3::NormalizeSafe(ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.direction", light.direction), { 0.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }), { 0.0f, -1.0f, 0.0f });
		light.position = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.position", light.position), { 0.0f, 0.0f, 0.0f }, { -10000.0f, -10000.0f, -10000.0f }, { 10000.0f, 10000.0f, 10000.0f });
		light.radius = ClampFinite(GetLightParameterOrDefault(parameters, "light0.radius", light.radius), 0.0f, 0.0f, 10000.0f);
		light.decay = ClampFinite(GetLightParameterOrDefault(parameters, "light0.decay", light.decay), 1.0f, 0.0f, 100.0f);
		light.distance = ClampFinite(GetLightParameterOrDefault(parameters, "light0.distance", light.distance), 10.0f, 0.0f, 10000.0f);
		light.cosFalloffStart = ClampFinite(GetLightParameterOrDefault(parameters, "light0.cosFalloffStart", light.cosFalloffStart), 1.0f, 0.0f, 1.0f);
		light.cosAngle = ClampFinite(GetLightParameterOrDefault(parameters, "light0.cosAngle", light.cosAngle), 0.5f, 0.0f, 1.0f);
		if (light.cosFalloffStart < light.cosAngle) { light.cosFalloffStart = light.cosAngle; }
		light.areaSize = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.areaSize", light.areaSize), { 2.0f, 2.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 10000.0f, 10000.0f, 10000.0f });
		shadowCasterLightIndex_ = std::clamp(shadowCasterLightIndex_, -1, static_cast<int32_t>(punctualLights_.size()) - 1);
		SyncLightParametersFromCurrentState(); // クランプや正規化後の値をParameterManager表示にも戻す。
	}

	void LightManager::SyncLightParametersFromCurrentState()
	{
		if (!lightParametersRegistered_)
		{
			return;
		}
		auto* parameters = ParameterManager::GetInstance();
		parameters->SetValue(kLightManagerGroup, "ambientColor", lightingSettings_.ambientColor);
		parameters->SetValue(kLightManagerGroup, "fogColor", lightingSettings_.fogColor);
		parameters->SetValue(kLightManagerGroup, "exposure", lightingSettings_.exposure);
		parameters->SetValue(kLightManagerGroup, "contrast", lightingSettings_.contrast);
		parameters->SetValue(kLightManagerGroup, "fogStart", lightingSettings_.fogStart);
		parameters->SetValue(kLightManagerGroup, "fogEnd", lightingSettings_.fogEnd);
		parameters->SetValue(kLightManagerGroup, "enableFog", lightingSettings_.enableFog != 0u);
		parameters->SetValue(kLightManagerGroup, "specularStrength", lightingSettings_.specularStrength);
		parameters->SetValue(kLightManagerGroup, "diffuseStrength", lightingSettings_.diffuseStrength);
		parameters->SetValue(kLightManagerGroup, "specularPowerScale", lightingSettings_.specularPowerScale);
		parameters->SetValue(kLightManagerGroup, "rimLightStrength", lightingSettings_.rimLightStrength);
		parameters->SetValue(kLightManagerGroup, "rimLightPower", lightingSettings_.rimLightPower);
		parameters->SetValue(kLightManagerGroup, "enableRimLight", lightingSettings_.enableRimLight != 0u);
		parameters->SetValue(kLightManagerGroup, "enableHalfLambert", lightingSettings_.enableHalfLambert != 0u);
		parameters->SetValue(kLightManagerGroup, "rimLightColor", lightingSettings_.rimLightColor);
		parameters->SetValue(kLightManagerGroup, "shadingMode", static_cast<int32_t>(lightingSettings_.shadingMode));

		parameters->SetValue(kLightManagerGroup, "enableShadow", enableShadow_);
		parameters->SetValue(kLightManagerGroup, "shadowBias", shadowBias_);
		parameters->SetValue(kLightManagerGroup, "normalBias", normalBias_);
		parameters->SetValue(kLightManagerGroup, "shadowStrength", shadowStrength_);
		parameters->SetValue(kLightManagerGroup, "shadowMapSize", static_cast<int32_t>(shadowMapSize_));
		parameters->SetValue(kLightManagerGroup, "showShadowMapDebug", showShadowMapDebug_);
		parameters->SetValue(kLightManagerGroup, "showShadowFactorDebug", showShadowFactorDebug_);
		parameters->SetValue(kLightManagerGroup, "shadowCasterLightIndex", shadowCasterLightIndex_);
		parameters->SetValue(kLightManagerGroup, "shadowFocusMode", static_cast<int32_t>(shadowFocusMode_));
		parameters->SetValue(kLightManagerGroup, "manualShadowFocusPosition", manualShadowFocusPosition_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowDistance", directionalShadowDistance_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowWidth", directionalShadowWidth_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowHeight", directionalShadowHeight_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowNearZ", directionalShadowNearZ_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowFarZ", directionalShadowFarZ_);
		parameters->SetValue(kLightManagerGroup, "directionalShadowFocusOffset", directionalShadowFocusOffset_);
		parameters->SetValue(kLightManagerGroup, "spotShadowNearZ", spotShadowNearZ_);

		if (punctualLights_.empty())
		{
			return;
		}
		const auto& light = punctualLights_.front();
		parameters->SetValue(kLightManagerGroup, "light0.lightType", static_cast<int32_t>(light.lightType));
		parameters->SetValue(kLightManagerGroup, "light0.enabled", light.enabled != 0u);
		parameters->SetValue(kLightManagerGroup, "light0.color", light.color);
		parameters->SetValue(kLightManagerGroup, "light0.intensity", light.intensity);
		parameters->SetValue(kLightManagerGroup, "light0.direction", light.direction);
		parameters->SetValue(kLightManagerGroup, "light0.position", light.position);
		parameters->SetValue(kLightManagerGroup, "light0.radius", light.radius);
		parameters->SetValue(kLightManagerGroup, "light0.decay", light.decay);
		parameters->SetValue(kLightManagerGroup, "light0.distance", light.distance);
		parameters->SetValue(kLightManagerGroup, "light0.cosFalloffStart", light.cosFalloffStart);
		parameters->SetValue(kLightManagerGroup, "light0.cosAngle", light.cosAngle);
		parameters->SetValue(kLightManagerGroup, "light0.areaSize", light.areaSize);
	}

	void LightManager::UnregisterLightParameters()
	{
		ParameterManager::GetInstance()->UnregisterParameterApplier(kLightManagerGroup, this); // Finalize後に破棄済みLightManagerへ反映しないよう解除する。
		lightParametersRegistered_ = false;
	}

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
		// 保存対象のライト/影/ライティング数値はParameters > LightManagerで一元編集する。
		ImGui::TextUnformatted("Editable lighting values are in Parameters > LightManager.");
		ImGui::Text("Ambient: (%.3f, %.3f, %.3f, %.3f)", lightingSettings_.ambientColor.x, lightingSettings_.ambientColor.y, lightingSettings_.ambientColor.z, lightingSettings_.ambientColor.w);
		ImGui::Text("Exposure / Contrast: %.3f / %.3f", lightingSettings_.exposure, lightingSettings_.contrast);
		ImGui::Text("Fog: %s  Start / End: %.2f / %.2f", lightingSettings_.enableFog != 0u ? "true" : "false", lightingSettings_.fogStart, lightingSettings_.fogEnd);
		ImGui::Text("Shading Mode: %u  Diffuse / Specular: %.3f / %.3f", lightingSettings_.shadingMode, lightingSettings_.diffuseStrength, lightingSettings_.specularStrength);
		ImGui::Text("Rim: %s  Strength / Power: %.3f / %.3f", lightingSettings_.enableRimLight != 0u ? "true" : "false", lightingSettings_.rimLightStrength, lightingSettings_.rimLightPower);

		if (ImGui::Button("+ Add Light"))
		{
			PunctualLightGPU L{};
			L.lightType = 1;
			L.color = { 1, 1, 1, 1 };
			L.intensity = 1.0f;
			L.direction = { 0, -1, 0 };
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

			const char* types[] = { "None", "Directional", "Point", "Spot", "RectArea", "SphereArea" };
			const uint32_t typeIndex = (L.lightType < static_cast<uint32_t>(IM_ARRAYSIZE(types))) ? L.lightType : 0u;
			ImGui::Text("Type: %s", types[typeIndex]);
			ImGui::Text("Enabled: %s", L.enabled != 0u ? "true" : "false");
			ImGui::Text("Color: (%.3f, %.3f, %.3f, %.3f)", L.color.x, L.color.y, L.color.z, L.color.w);
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", L.position.x, L.position.y, L.position.z);
			ImGui::Text("Radius / Decay: %.2f / %.2f", L.radius, L.decay);
			ImGui::Text("Spot cosInner / cosOuter: %.3f / %.3f", L.cosFalloffStart, L.cosAngle);
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
		ImGui::SeparatorText("Shadow Frustum");
		ImGui::Text("Shadow Enabled: %s", enableShadow_ ? "true" : "false");
		ImGui::Text("Shadow Debug Map / Factor: %s / %s", showShadowMapDebug_ ? "true" : "false", showShadowFactorDebug_ ? "true" : "false");
		ImGui::Text("Shadow Focus Mode: %u", static_cast<uint32_t>(shadowFocusMode_));
		ImGui::Text("Manual Shadow Focus Position: (%.2f, %.2f, %.2f)", manualShadowFocusPosition_.x, manualShadowFocusPosition_.y, manualShadowFocusPosition_.z);
		ImGui::Text("Shadow Focus Offset: %.2f", directionalShadowFocusOffset_);
		ImGui::Text("Spot Shadow NearZ: %.3f", spotShadowNearZ_);
		ImGui::Text("Shadow Caster Light Index: %d", shadowCasterLightIndex_);
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
		ImGui::Text("Active Shadow Caster Type: %s", activeCasterName);
		int32_t activeLightIndex = -1;
		PunctualLightGPU activeLight{};
		ShadowCasterType activeType = ShadowCasterType::None;
		const bool hasActiveLight = TryGetActiveShadowCasterLightInfo(activeLightIndex, activeLight, activeType);
		ImGui::Text("Active Shadow Light Index: %d", hasActiveLight ? activeLightIndex : -1);
		ImGui::Text("Active Shadow Light Direction: (%.3f, %.3f, %.3f)", hasActiveLight ? activeLight.direction.x : 0.0f, hasActiveLight ? activeLight.direction.y : 0.0f, hasActiveLight ? activeLight.direction.z : 0.0f);
		ImGui::Text("Active Shadow Light Enabled: %s", (hasActiveLight && activeLight.enabled != 0u) ? "true" : "false");
		ImGui::Text("Active Shadow Light Intensity: %.3f", hasActiveLight ? activeLight.intensity : 0.0f);
		ImGui::Text("Shadow Focus Position: (%.2f, %.2f, %.2f)", currentShadowFocusPosition_.x, currentShadowFocusPosition_.y, currentShadowFocusPosition_.z);
		ImGui::Text("Shadow Direction: (%.3f, %.3f, %.3f)", currentShadowDirection_.x, currentShadowDirection_.y, currentShadowDirection_.z);
		ImGui::Text("Shadow Distance: %.2f", directionalShadowDistance_);
		ImGui::Text("Shadow Width / Height: %.2f / %.2f", directionalShadowWidth_, directionalShadowHeight_);
		ImGui::Text("Shadow Near / Far: %.3f / %.2f", directionalShadowNearZ_, directionalShadowFarZ_);
		ImGui::Text("Applied Shadow Width / Height: %.2f / %.2f", currentShadowFrustumWidth_, currentShadowFrustumHeight_);
		ImGui::Text("Applied Shadow Near / Far: %.3f / %.2f", currentShadowFrustumNearZ_, currentShadowFrustumFarZ_);
		ImGui::Text("Shadow Map Size: %u", shadowMapSize_);
		ImGui::Text("Shadow Bias / Normal Bias: %.6f / %.4f", shadowBias_, normalBias_);
		ImGui::Text("Active Lights (type!=0): will be uploaded");
#endif // USE_IMGUI
	}


	LightManager::ShadowCasterType LightManager::GetActiveShadowCasterType() const
	{
		int32_t activeIndex = -1;
		PunctualLightGPU activeLight{};
		ShadowCasterType activeType = ShadowCasterType::None;
		if (TryGetActiveShadowCasterLightInfo(activeIndex, activeLight, activeType))
		{
			return activeType;
		}
		return ShadowCasterType::None;
	}

	bool LightManager::TryGetActiveShadowCasterLightInfo(int32_t& outIndex, PunctualLightGPU& outLight, ShadowCasterType& outType) const
	{
		const auto isCandidate = [](const PunctualLightGPU& light)
			{
				return light.intensity > 0.0f && light.enabled != 0u && (light.lightType == 1 || light.lightType == 3);
			};
		if (shadowCasterLightIndex_ >= 0 && shadowCasterLightIndex_ < static_cast<int32_t>(punctualLights_.size()))
		{
			const auto& selected = punctualLights_[shadowCasterLightIndex_];
			if (isCandidate(selected))
			{
				outIndex = shadowCasterLightIndex_;
				outLight = selected;
				outType = (selected.lightType == 3) ? ShadowCasterType::Spot : ShadowCasterType::Directional;
				return true;
			}
		}
		for (int32_t i = 0; i < static_cast<int32_t>(punctualLights_.size()); ++i)
		{
			const auto& light = punctualLights_[i];
			// Shadowに使うライト選択条件を統一して、無効ライトが影行列に使われないようにする
			if (!isCandidate(light)) { continue; }
			outIndex = i;
			outLight = light;
			outType = (light.lightType == 3) ? ShadowCasterType::Spot : ShadowCasterType::Directional;
			return true;
		}
		return false;
	}

	Matrix4x4 LightManager::BuildShadowLightViewProjection(const Vector3& focusPosition) const
	{
		Vector3 lightDir = { 0.3f, -1.0f, 0.2f };
		int32_t activeIndex = -1;
		PunctualLightGPU activeLight{};
		ShadowCasterType casterType = ShadowCasterType::None;
		if (TryGetActiveShadowCasterLightInfo(activeIndex, activeLight, casterType))
		{
			if (casterType == ShadowCasterType::Directional)
			{
				lightDir = Vector3::Normalize(activeLight.direction);
			}
		}

		if (casterType == ShadowCasterType::Spot)
		{
			const auto& light = activeLight;
			const float spotDistance = std::max(light.distance, 5.0f);
			const float spotOuterCos = std::clamp(light.cosAngle, 0.01f, 0.999f);
			const float outerAngle = std::acos(spotOuterCos) * 2.0f;
			const float fovY = std::clamp(outerAngle, 0.15f, 3.0f);
			const Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(light.position, light.position + Vector3::Normalize(light.direction), { 0.0f, 1.0f, 0.0f });
			const Matrix4x4 proj = Matrix4x4::MakePerspectiveFovMatrix(fovY, 1.0f, spotShadowNearZ_, spotDistance);
			return Matrix4x4::Multiply(view, proj);
		}

		Vector3 directionalFocusPosition = focusPosition;
		if (shadowFocusMode_ == ShadowFocusMode::Manual || shadowFocusMode_ == ShadowFocusMode::StageCenter)
		{
			// Shadow Frustumの中心を調整できるようにして、ステージ全体を影範囲に収めやすくする
			directionalFocusPosition = manualShadowFocusPosition_;
		}
		directionalFocusPosition += Vector3{ 0.0f, directionalShadowFocusOffset_, 0.0f };
		const float shadowWidth = std::max(std::fabs(directionalShadowWidth_), 0.01f);
		const float shadowHeight = std::max(std::fabs(directionalShadowHeight_), 0.01f);
		const float shadowNear = std::clamp(directionalShadowNearZ_, 0.01f, 500.0f);
		const float shadowFar = std::max(directionalShadowFarZ_, shadowNear + 1.0f);
		// Shadow Frustum debugは実際のLightViewProjectionから復元し、サイズに関係なく描画する
		const Matrix4x4 lightViewProjection = Matrix4x4::MakeLightViewProjection(
			lightDir, directionalFocusPosition, directionalShadowDistance_, shadowWidth, shadowHeight, shadowNear, shadowFar);
		currentShadowFocusPosition_ = directionalFocusPosition;
		currentShadowDirection_ = lightDir;
		currentShadowLightViewProjection_ = lightViewProjection;
		currentShadowFrustumWidth_ = shadowWidth;
		currentShadowFrustumHeight_ = shadowHeight;
		currentShadowFrustumNearZ_ = shadowNear;
		currentShadowFrustumFarZ_ = shadowFar;
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
			static char presetId[64] = "default_light";
			ImGui::InputText("LightPreset Id", presetId, IM_ARRAYSIZE(presetId));
			if (ImGui::Button("Save Current LightPreset")) { SaveLightPreset(presetId); }
			if (ImGui::Button("Apply Selected LightPreset")) { ApplyLightPresetByPath(std::string("Resources/DataAssets/LightPresets/") + presetId + ".json"); }
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
			// HLSLのLightingSettingsへ最新のAmbient/Exposure/Contrast/Fog/Shadingを送る。
			*lightingSettingsData_ = lightingSettings_;
		}

		commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b5, lightingSettingsResource_->GetGPUVirtualAddress());
	}

	void LightManager::AddDefaultDirectionalLight()
	{
		PunctualLightGPU light{};
		light.lightType = 1; // Directional
		light.color = { 1.0f, 0.97f, 0.92f, 1.0f };
		light.intensity = 0.90f;
		light.direction = Vector3::Normalize({ 0.45f, -1.0f, 0.35f });
		light.enabled = 1u;

		punctualLights_.clear();
		punctualLights_.push_back(light);
		lightingSettings_ = LightingSettingsGPU{};
		lightingSettings_.ambientColor = { 0.10f, 0.11f, 0.13f, 0.18f };
		lightingSettings_.diffuseStrength = 0.95f;
		lightingSettings_.specularStrength = 0.10f;
		lightingSettings_.rimLightStrength = 0.55f;
	}

	void LightManager::ResetToDefaultLighting()
	{
		punctualLights_.clear();
		// 保存済み設定を優先し、ファイルがない場合だけ確認用の初期ライトを生成する。
		if (!ApplyLightPresetByPath("Resources/DataAssets/LightPresets/default_light.json"))
		{
			AddDefaultDirectionalLight();
		}
	}

	bool LightManager::SaveLightPreset(const std::string& assetId)
	{
		JsonAssetEntry entry{};
		entry.type = "LightPreset";
		entry.id = assetId;
		entry.displayName = assetId;
		entry.path = "Resources/DataAssets/LightPresets/" + assetId + ".json";
		LightPreset preset{};
		if (!punctualLights_.empty())
		{
			const auto& light = punctualLights_.front();
			preset.directionalDirection = light.direction;
			preset.color = light.color;
			preset.intensity = light.intensity;
		}
		preset.enableShadow = enableShadow_;
		preset.shadowBias = shadowBias_;
		preset.normalBias = normalBias_;
		preset.shadowStrength = shadowStrength_;
		preset.shadowMapSize = shadowMapSize_;
		preset.shadowWidth = directionalShadowWidth_;
		preset.shadowHeight = directionalShadowHeight_;
		preset.shadowNearZ = directionalShadowNearZ_;
		preset.shadowFarZ = directionalShadowFarZ_;
		preset.shadowFocusMode = static_cast<uint32_t>(shadowFocusMode_);
		preset.ambientColor = lightingSettings_.ambientColor;
		preset.fogColor = lightingSettings_.fogColor;
		preset.exposure = lightingSettings_.exposure;
		preset.contrast = lightingSettings_.contrast;
		preset.fogStart = lightingSettings_.fogStart;
		preset.fogEnd = lightingSettings_.fogEnd;
		preset.enableFog = lightingSettings_.enableFog;
		preset.specularStrength = lightingSettings_.specularStrength;
		preset.diffuseStrength = lightingSettings_.diffuseStrength;
		preset.specularPowerScale = lightingSettings_.specularPowerScale;
		preset.rimLightStrength = lightingSettings_.rimLightStrength;
		preset.rimLightPower = lightingSettings_.rimLightPower;
		preset.enableRimLight = lightingSettings_.enableRimLight;
		preset.enableHalfLambert = lightingSettings_.enableHalfLambert;
		preset.rimLightColor = lightingSettings_.rimLightColor;
		preset.shadingMode = lightingSettings_.shadingMode;
		preset.ToJson(entry.data);
		return JsonDataManager::SafeSave(entry);
	}

	bool LightManager::ApplyLightPresetByPath(const std::string& filePath)
	{
		JsonAssetEntry entry{};
		if (!JsonDataManager::SafeLoad(filePath, entry))
		{
			return false;
		}
		LightPreset preset{};
		preset.FromJson(entry.data);
		if (punctualLights_.empty())
		{
			AddDefaultDirectionalLight();
		}
		auto& light = punctualLights_.front();
		light.lightType = 1;
		light.enabled = 1;
		light.direction = preset.directionalDirection;
		light.color = preset.color;
		light.intensity = preset.intensity;
		enableShadow_ = preset.enableShadow;
		shadowBias_ = preset.shadowBias;
		normalBias_ = preset.normalBias;
		shadowStrength_ = preset.shadowStrength;
		shadowMapSize_ = preset.shadowMapSize;
		directionalShadowWidth_ = preset.shadowWidth;
		directionalShadowHeight_ = preset.shadowHeight;
		directionalShadowNearZ_ = preset.shadowNearZ;
		directionalShadowFarZ_ = preset.shadowFarZ;
		shadowFocusMode_ = static_cast<ShadowFocusMode>(preset.shadowFocusMode);
		lightingSettings_.ambientColor = preset.ambientColor;
		lightingSettings_.fogColor = preset.fogColor;
		lightingSettings_.exposure = preset.exposure;
		lightingSettings_.contrast = preset.contrast;
		lightingSettings_.fogStart = preset.fogStart;
		lightingSettings_.fogEnd = preset.fogEnd;
		lightingSettings_.enableFog = preset.enableFog;
		lightingSettings_.specularStrength = preset.specularStrength;
		lightingSettings_.diffuseStrength = preset.diffuseStrength;
		lightingSettings_.specularPowerScale = preset.specularPowerScale;
		lightingSettings_.rimLightStrength = preset.rimLightStrength;
		lightingSettings_.rimLightPower = preset.rimLightPower;
		lightingSettings_.enableRimLight = preset.enableRimLight;
		lightingSettings_.enableHalfLambert = preset.enableHalfLambert;
		lightingSettings_.rimLightColor = preset.rimLightColor;
		lightingSettings_.shadingMode = preset.shadingMode;

		// プリセットJSON由来の不正値も描画リソースへ渡る前に安全範囲へ補正する。
		lightingSettings_.ambientColor = SanitizeVector4(lightingSettings_.ambientColor, LightingSettingsGPU{}.ambientColor);
		lightingSettings_.fogColor = SanitizeVector4(lightingSettings_.fogColor, LightingSettingsGPU{}.fogColor);
		lightingSettings_.exposure = ClampFinite(lightingSettings_.exposure, 1.0f, 0.05f, 8.0f);
		lightingSettings_.contrast = ClampFinite(lightingSettings_.contrast, 1.0f, 0.05f, 4.0f);
		lightingSettings_.fogStart = ClampFinite(lightingSettings_.fogStart, 45.0f, 0.0f, 10000.0f);
		lightingSettings_.fogEnd = ClampFinite(lightingSettings_.fogEnd, 140.0f, lightingSettings_.fogStart + 1.0f, 20000.0f);
		lightingSettings_.specularStrength = ClampFinite(lightingSettings_.specularStrength, 0.08f, 0.0f, 4.0f);
		lightingSettings_.diffuseStrength = ClampFinite(lightingSettings_.diffuseStrength, 1.0f, 0.0f, 8.0f);
		lightingSettings_.specularPowerScale = ClampFinite(lightingSettings_.specularPowerScale, 1.0f, 0.01f, 16.0f);
		lightingSettings_.rimLightStrength = ClampFinite(lightingSettings_.rimLightStrength, 0.0f, 0.0f, 8.0f);
		lightingSettings_.rimLightPower = ClampFinite(lightingSettings_.rimLightPower, 2.0f, 0.01f, 32.0f);
		lightingSettings_.rimLightColor = SanitizeVector4(lightingSettings_.rimLightColor, LightingSettingsGPU{}.rimLightColor);
		lightingSettings_.shadingMode = std::clamp(lightingSettings_.shadingMode, 0u, 2u);
		shadowBias_ = ClampFinite(shadowBias_, 0.0f, 0.0f, 0.1f);
		normalBias_ = ClampFinite(normalBias_, 0.025f, 0.0f, 1.0f);
		shadowStrength_ = ClampFinite(shadowStrength_, 0.6f, 0.0f, 1.0f);
		const uint32_t sanitizedShadowMapSize = NormalizeShadowMapSize(static_cast<int32_t>(shadowMapSize_));
		directionalShadowWidth_ = ClampFinite(directionalShadowWidth_, 35.0f, 1.0f, 5000.0f);
		directionalShadowHeight_ = ClampFinite(directionalShadowHeight_, 35.0f, 1.0f, 5000.0f);
		directionalShadowNearZ_ = ClampFinite(directionalShadowNearZ_, 0.1f, 0.001f, 5000.0f);
		directionalShadowFarZ_ = ClampFinite(directionalShadowFarZ_, 120.0f, directionalShadowNearZ_ + 0.001f, 20000.0f);
		shadowFocusMode_ = static_cast<ShadowFocusMode>(std::clamp(static_cast<int32_t>(shadowFocusMode_), 0, 3));
		light.direction = Vector3::NormalizeSafe(light.direction, { 0.0f, -1.0f, 0.0f });
		light.color = SanitizeVector4(light.color, { 1.0f, 1.0f, 1.0f, 1.0f });
		light.intensity = ClampFinite(light.intensity, 1.0f, 0.0f, 100.0f);
		if (dxCommon_ && shadowMapSize_ != sanitizedShadowMapSize)
		{
			shadowMapSize_ = sanitizedShadowMapSize;
			dxCommon_->SetShadowMapSize(shadowMapSize_, shadowMapSize_);
		}
		else
		{
			shadowMapSize_ = sanitizedShadowMapSize;
		}
		SyncLightParametersFromCurrentState(); // 既存プリセット適用後もParameters側の表示と保存値候補を最新化する。
		return true;
	}
} // namespace Ken4lowEngine
