#include "ModelDisintegrationEffect.h"

#include "Model.h"
#include "ModelManager.h"

#include <algorithm>
#include <cmath>
#include <random>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}
}

void ModelDisintegrationEffect::Initialize()
{
	particles_.clear();
	isActive_ = false;
	elapsedTime_ = 0.0f;
}

void ModelDisintegrationEffect::PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	auto model = K4E::ModelManager::GetInstance()->LoadModel(modelPath);
	if (!model) { return; }

	DisintegrationEmitter::Settings settings{};
	settings.particleCount = parameters_.particleCount;
	settings.particleSize = parameters_.particleSize;
	settings.lifeTime = parameters_.lifeTime;
	settings.spreadPower = parameters_.spreadPower;
	settings.upwardPower = parameters_.upwardPower;
	settings.startDelay = parameters_.startDelay;

	particles_ = emitter_.EmitFromModel(model->GetModelData(), worldMatrix, settings);
	isActive_ = !particles_.empty();
	elapsedTime_ = 0.0f;
}

void ModelDisintegrationEffect::Update(float deltaTime)
{
	if (!isActive_) { return; }

	elapsedTime_ += deltaTime;
	bool anyAlive = false;

	for (auto& particle : particles_)
	{
		if (!particle.alive) { continue; }

		particle.age += deltaTime;
		if (particle.age < particle.startDelay)
		{
			anyAlive = true;
			continue;
		}

		const float activeAge = particle.age - particle.startDelay;
		const float preserveRate = Clamp01(activeAge / std::max(parameters_.shapePreserveTime, 0.0001f));
		const K4E::Vector3 noise = RandomNoiseVector() * parameters_.noisePower;

		particle.velocity += (particle.outward * parameters_.spreadPower + noise) * (deltaTime * preserveRate);
		particle.velocity.y += (parameters_.upwardPower + parameters_.gravity) * deltaTime;
		particle.position += particle.velocity * (deltaTime * preserveRate);

		const float lifeRate = Clamp01(activeAge / std::max(particle.life, 0.0001f));
		particle.alpha = Clamp01(1.0f - lifeRate * parameters_.fadeSpeed);

		if (activeAge >= particle.life || particle.alpha <= 0.0f)
		{
			particle.alive = false;
			continue;
		}

		anyAlive = true;
	}

	if (!anyAlive)
	{
		StopIfFinished();
	}
}

void ModelDisintegrationEffect::Draw() const
{
	if (!isActive_) { return; }
	renderer_.Draw(particles_);
}

void ModelDisintegrationEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Model Disintegration Effect");
	ImGui::Text("Active: %s", isActive_ ? "true" : "false");
	ImGui::Text("Particle Instances: %zu", particles_.size());
	ImGui::SliderInt("particleCount", &parameters_.particleCount, 32, 8000);
	ImGui::SliderFloat("particleSize", &parameters_.particleSize, 0.005f, 0.20f);
	ImGui::SliderFloat("lifeTime", &parameters_.lifeTime, 0.10f, 8.0f);
	ImGui::SliderFloat("spreadPower", &parameters_.spreadPower, 0.0f, 8.0f);
	ImGui::SliderFloat("gravity", &parameters_.gravity, -10.0f, 10.0f);
	ImGui::SliderFloat("noisePower", &parameters_.noisePower, 0.0f, 8.0f);
	ImGui::SliderFloat("fadeSpeed", &parameters_.fadeSpeed, 0.1f, 4.0f);
	ImGui::SliderFloat("upwardPower", &parameters_.upwardPower, -4.0f, 8.0f);
	ImGui::SliderFloat("startDelay", &parameters_.startDelay, 0.0f, 3.0f);
	ImGui::SliderFloat("shapePreserveTime", &parameters_.shapePreserveTime, 0.0f, 3.0f);
	ImGui::Text("Debug Key: F9 plays Characters/body.gltf at the player position.");
	ImGui::End();
#endif
}

K4E::Vector3 ModelDisintegrationEffect::RandomNoiseVector()
{
	static std::mt19937 rng{ 0xA5A5D157u };
	static std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };
	return K4E::Vector3::NormalizeSafe({ dist(rng), dist(rng), dist(rng) }, { 0.0f, 1.0f, 0.0f });
}

void ModelDisintegrationEffect::StopIfFinished()
{
	isActive_ = false;
	particles_.clear();
}
