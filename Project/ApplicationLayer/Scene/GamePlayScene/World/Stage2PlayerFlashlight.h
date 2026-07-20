#pragma once

#include <CameraManager.h>
#include <Input.h>
#include <LightManager.h>

#include <limits>

namespace K4E = ::Ken4lowEngine;

/// Stage 2だけで有効になる、Main Camera追従型の懐中電灯Spot Light。
class Stage2PlayerFlashlight final
{
public:
	void Update()
	{
		EnsureLight();
		auto* lightManager = K4E::LightManager::GetInstance();
		auto* cameraManager = K4E::CameraManager::GetInstance();
		auto* input = K4E::Input::GetInstance();
		if (!lightManager || !cameraManager || !input) return;
		if (input->TriggerKey(DIK_F)) enabled_ = !enabled_;

		auto& lights = lightManager->GetMutablePunctualLightsForEditor();
		if (lightIndex_ >= lights.size()) return;
		K4E::LightManager::PunctualLightGPU& light = lights[lightIndex_];
		const K4E::Vector3 forward = K4E::Vector3::NormalizeSafe(
			cameraManager->GetActiveCameraForward(),
			{ 0.0f, 0.0f, 1.0f });
		light.position = cameraManager->GetActiveCameraPosition() + forward * 0.35f + K4E::Vector3{ 0.0f, -0.10f, 0.0f };
		light.direction = forward;
		light.enabled = enabled_ ? 1u : 0u; // Fキー切替とCamera追従を同じライト要素へ反映する。
	}

private:
	void EnsureLight()
	{
		auto* lightManager = K4E::LightManager::GetInstance();
		if (!lightManager) return;
		auto& lights = lightManager->GetMutablePunctualLightsForEditor();
		if (lightIndex_ < lights.size()) return;

		K4E::LightManager::PunctualLightGPU light{};
		light.lightType = 3u;
		light.color = { 0.92f, 0.96f, 1.0f, 1.0f };
		light.intensity = 3.6f;
		light.distance = 86.0f;
		light.decay = 1.10f;
		light.cosFalloffStart = 0.95f;
		light.cosAngle = 0.78f;
		light.direction = { 0.0f, 0.0f, 1.0f };
		light.enabled = 1u;
		lightIndex_ = lights.size();
		lights.push_back(light); // 環境灯を増やさず、Playerが向けた方向だけを照らす。
	}

	size_t lightIndex_ = std::numeric_limits<size_t>::max();
	bool enabled_ = true;
};
