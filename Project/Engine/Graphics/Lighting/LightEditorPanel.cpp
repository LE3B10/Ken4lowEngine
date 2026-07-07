#define NOMINMAX
#include "LightEditorPanel.h"

#include "LightManager.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

		Vector3 DirectionToEulerDegForLightEditor(const Vector3& dir)
		{
			Vector3 n = Vector3::Normalize(dir);
			const float pitch = std::asin(-n.y);
			const float yaw = std::atan2(n.x, n.z);
			return { pitch * kRadToDeg, yaw * kRadToDeg, 0.0f };
		}
	}

	void LightEditorPanel::Draw(LightManager& lightManager, bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのLight Editor表示フラグが閉じている間は、Runtime側のLightManagerへUI負荷を持ち込まない。
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		// LightManagerはFacadeとして入口だけ残し、ImGuiウィンドウ構築はLightEditorPanelが担当する。
		ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Light Editor", pOpen))
		{
			if (ImGui::CollapsingHeader("Punctual Lights"))
			{
				DrawPunctualLightsInspector(lightManager);
			}
			static char presetId[64] = "default_light";
			ImGui::InputText("LightPreset Id", presetId, IM_ARRAYSIZE(presetId));
			if (ImGui::Button("Save Current LightPreset")) { lightManager.SaveLightPreset(presetId); }
			if (ImGui::Button("Apply Selected LightPreset")) { lightManager.ApplyLightPresetByPath(std::string("Resources/DataAssets/LightPresets/") + presetId + ".json"); }
		}
		ImGui::End();
#else
		(void)lightManager;
		(void)pOpen;
#endif // USE_IMGUI
	}

	void LightEditorPanel::DrawPunctualLightsInspector(LightManager& lightManager)
	{
#ifdef USE_IMGUI
		// Detailsと専用Light Editorの内容差分をなくすため、Punctual Lights本体の描画をここへ集約する。
		ImGui::Text("Light Count: %zu", lightManager.punctualLights_.size());
		if (!lightManager.punctualLights_.empty())
		{
			const auto& first = lightManager.punctualLights_.front();
			const char* summaryTypes[] = { "None", "Directional", "Point", "Spot", "RectArea", "SphereArea" };
			const uint32_t typeIndex = (first.lightType < static_cast<uint32_t>(IM_ARRAYSIZE(summaryTypes))) ? first.lightType : 0;
			Vector3 eulerDeg = DirectionToEulerDegForLightEditor(first.direction);
			ImGui::Text("Light #0 Type: %s", summaryTypes[typeIndex]);
			ImGui::Text("Light #0 Color: (%.3f, %.3f, %.3f, %.3f)", first.color.x, first.color.y, first.color.z, first.color.w);
			ImGui::Text("Light #0 Intensity: %.3f", first.intensity);
			ImGui::Text("Light #0 Pitch / Yaw / Roll: %.1f / %.1f / %.1f", eulerDeg.x, eulerDeg.y, eulerDeg.z);
			ImGui::Text("Light #0 Direction: (%.3f, %.3f, %.3f)", first.direction.x, first.direction.y, first.direction.z);
		}
		ImGui::Separator();
		// 保存対象のライト/影/ライティング数値はParameters > LightManagerで一元編集する。
		ImGui::TextUnformatted("Editable lighting values are in Parameters > LightManager.");
		ImGui::Text("Ambient: (%.3f, %.3f, %.3f, %.3f)", lightManager.lightingSettings_.ambientColor.x, lightManager.lightingSettings_.ambientColor.y, lightManager.lightingSettings_.ambientColor.z, lightManager.lightingSettings_.ambientColor.w);
		ImGui::Text("Exposure / Contrast: %.3f / %.3f", lightManager.lightingSettings_.exposure, lightManager.lightingSettings_.contrast);
		ImGui::Text("Fog: %s  Start / End: %.2f / %.2f", lightManager.lightingSettings_.enableFog != 0u ? "true" : "false", lightManager.lightingSettings_.fogStart, lightManager.lightingSettings_.fogEnd);
		ImGui::Text("Shading Mode: %u  Diffuse / Specular: %.3f / %.3f", lightManager.lightingSettings_.shadingMode, lightManager.lightingSettings_.diffuseStrength, lightManager.lightingSettings_.specularStrength);
		ImGui::Text("Rim: %s  Strength / Power: %.3f / %.3f", lightManager.lightingSettings_.enableRimLight != 0u ? "true" : "false", lightManager.lightingSettings_.rimLightStrength, lightManager.lightingSettings_.rimLightPower);

		if (ImGui::Button("+ Add Light"))
		{
			LightManager::PunctualLightGPU L{};
			L.lightType = 1;
			L.color = { 1, 1, 1, 1 };
			L.intensity = 1.0f;
			L.direction = { 0, -1, 0 };
			L.areaSize = { 2.0f, 2.0f, 1.0f };
			L.distance = 10.0f;
			L.decay = 1.0f;
			L.enabled = 1u;
			lightManager.punctualLights_.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All"))
		{
			lightManager.punctualLights_.clear();
		}

		for (size_t i = 0; i < lightManager.punctualLights_.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			auto& L = lightManager.punctualLights_[i];

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
				lightManager.punctualLights_.erase(lightManager.punctualLights_.begin() + i);
				ImGui::PopID();
				--i;
				continue;
			}
			ImGui::PopID();
		}

		ImGui::Separator();
		const bool hasPointLight = std::any_of(lightManager.punctualLights_.begin(), lightManager.punctualLights_.end(), [](const LightManager::PunctualLightGPU& light) { return light.lightType == 2 && light.intensity > 0.0f && light.enabled != 0u; });
		const bool hasAreaLight = std::any_of(lightManager.punctualLights_.begin(), lightManager.punctualLights_.end(), [](const LightManager::PunctualLightGPU& light) { return (light.lightType == 4 || light.lightType == 5) && light.intensity > 0.0f && light.enabled != 0u; });
		ImGui::SeparatorText("Shadow Frustum");
		ImGui::Text("Shadow Enabled: %s", lightManager.enableShadow_ ? "true" : "false");
		ImGui::Text("Shadow Debug Map / Factor: %s / %s", lightManager.showShadowMapDebug_ ? "true" : "false", lightManager.showShadowFactorDebug_ ? "true" : "false");
		ImGui::Text("Shadow Focus Mode: %u", static_cast<uint32_t>(lightManager.shadowFocusMode_));
		ImGui::Text("Manual Shadow Focus Position: (%.2f, %.2f, %.2f)", lightManager.manualShadowFocusPosition_.x, lightManager.manualShadowFocusPosition_.y, lightManager.manualShadowFocusPosition_.z);
		ImGui::Text("Shadow Focus Offset: %.2f", lightManager.directionalShadowFocusOffset_);
		ImGui::Text("Spot Shadow NearZ: %.3f", lightManager.spotShadowNearZ_);
		ImGui::Text("Shadow Caster Light Index: %d", lightManager.shadowCasterLightIndex_);
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
		for (const auto& L : lightManager.punctualLights_)
		{
			if (L.lightType == 3)
			{
				ImGui::Text("Spot cone cosOuter/cosInner: %.3f / %.3f", L.cosAngle, L.cosFalloffStart);
				ImGui::Text("Spot range: %.2f", L.distance);
				break;
			}
		}
		const LightManager::ShadowCasterType casterType = lightManager.GetActiveShadowCasterType();
		if (casterType == LightManager::ShadowCasterType::Directional)
		{
			ImGui::Text("Directional: LightViewProjection active");
		}
		else if (casterType == LightManager::ShadowCasterType::Spot)
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
		const char* activeCasterName = (casterType == LightManager::ShadowCasterType::Directional) ? "Directional" : (casterType == LightManager::ShadowCasterType::Spot) ? "Spot" : "None";
		ImGui::SeparatorText("Shadow Debug");
		ImGui::Text("Active Shadow Caster Type: %s", activeCasterName);
		int32_t activeLightIndex = -1;
		LightManager::PunctualLightGPU activeLight{};
		LightManager::ShadowCasterType activeType = LightManager::ShadowCasterType::None;
		const bool hasActiveLight = lightManager.TryGetActiveShadowCasterLightInfo(activeLightIndex, activeLight, activeType);
		ImGui::Text("Active Shadow Light Index: %d", hasActiveLight ? activeLightIndex : -1);
		ImGui::Text("Active Shadow Light Direction: (%.3f, %.3f, %.3f)", hasActiveLight ? activeLight.direction.x : 0.0f, hasActiveLight ? activeLight.direction.y : 0.0f, hasActiveLight ? activeLight.direction.z : 0.0f);
		ImGui::Text("Active Shadow Light Enabled: %s", (hasActiveLight && activeLight.enabled != 0u) ? "true" : "false");
		ImGui::Text("Active Shadow Light Intensity: %.3f", hasActiveLight ? activeLight.intensity : 0.0f);
		ImGui::Text("Shadow Focus Position: (%.2f, %.2f, %.2f)", lightManager.currentShadowFocusPosition_.x, lightManager.currentShadowFocusPosition_.y, lightManager.currentShadowFocusPosition_.z);
		ImGui::Text("Shadow Direction: (%.3f, %.3f, %.3f)", lightManager.currentShadowDirection_.x, lightManager.currentShadowDirection_.y, lightManager.currentShadowDirection_.z);
		ImGui::Text("Shadow Distance: %.2f", lightManager.directionalShadowDistance_);
		ImGui::Text("Shadow Width / Height: %.2f / %.2f", lightManager.directionalShadowWidth_, lightManager.directionalShadowHeight_);
		ImGui::Text("Shadow Near / Far: %.3f / %.2f", lightManager.directionalShadowNearZ_, lightManager.directionalShadowFarZ_);
		ImGui::Text("Applied Shadow Width / Height: %.2f / %.2f", lightManager.currentShadowFrustumWidth_, lightManager.currentShadowFrustumHeight_);
		ImGui::Text("Applied Shadow Near / Far: %.3f / %.2f", lightManager.currentShadowFrustumNearZ_, lightManager.currentShadowFrustumFarZ_);
		ImGui::Text("Shadow Map Size: %u", lightManager.shadowMapSize_);
		ImGui::Text("Shadow Bias / Normal Bias: %.6f / %.4f", lightManager.shadowBias_, lightManager.normalBias_);
		ImGui::Text("Active Lights (type!=0): will be uploaded");
#else
		(void)lightManager;
#endif // USE_IMGUI
	}
}
