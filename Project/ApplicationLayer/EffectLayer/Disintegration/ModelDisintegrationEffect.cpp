#define NOMINMAX
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
	settings.blockRotationRandomness = parameters_.blockRotationRandomness;
	settings.surfaceSampling = parameters_.surfaceSampling;
	settings.lifeTime = parameters_.lifeTime;
	settings.spreadPower = parameters_.spreadPower;
	settings.upwardPower = parameters_.upwardPower;
	settings.startDelay = parameters_.startDelay;
	settings.baseColor = parameters_.baseColor;
	settings.colorVariation = parameters_.colorVariation;

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
		if (parameters_.preserveShape && activeAge < parameters_.shapePreserveTime)
		{
			// 崩壊開始までは初期位置へ引き戻し、粒子群のシルエットをモデル表面に固定する。
			particle.velocity *= std::pow(0.08f, deltaTime);
			particle.position += (particle.initialPosition - particle.position) * Clamp01(deltaTime * 12.0f);
			particle.alpha = 1.0f;
			anyAlive = true;
			continue;
		}

		const float disintegrationAge = activeAge - (parameters_.preserveShape ? parameters_.shapePreserveTime : 0.0f);
		const float disintegrationRate = Clamp01(disintegrationAge / std::max(particle.life, 0.0001f));
		const K4E::Vector3 noise = RandomNoiseVector() * parameters_.noisePower;
		const K4E::Vector3 downward = { 0.0f, parameters_.gravity, 0.0f };

		// ブロック粒子を下方向・外方向・ランダム方向へ崩す力として collapsePower をまとめて掛ける。
		particle.velocity += (particle.outward * parameters_.spreadPower + noise + downward) * parameters_.collapsePower * deltaTime;
		particle.velocity.y += parameters_.upwardPower * parameters_.collapsePower * deltaTime;
		particle.position += particle.velocity * deltaTime;
		particle.rotation += particle.rotationVelocity * parameters_.blockRotationRandomness * deltaTime;
		particle.size *= std::pow(0.96f, deltaTime);
		particle.alpha = Clamp01(1.0f - disintegrationRate * parameters_.fadeSpeed);

		if (disintegrationAge >= particle.life || particle.alpha <= 0.0f)
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

void ModelDisintegrationEffect::Draw()
{
	if (!isActive_) { return; }
	renderer_.Draw(particles_);
}

void ModelDisintegrationEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("モデル崩壊エフェクト詳細");
	ImGui::Text("再生中: %s", isActive_ ? "はい" : "いいえ");
	ImGui::Text("パーティクル数: %zu", particles_.size());
	ImGui::SliderInt("パーティクル数", &parameters_.particleCount, 32, 8000);
	ImGui::Checkbox("ブロックモード", &parameters_.blockMode);
	if (ImGui::SliderFloat("パーティクルサイズ", &parameters_.particleSize, 0.005f, 0.30f))
	{
		parameters_.blockSize = parameters_.particleSize;
	}
	if (ImGui::SliderFloat("ブロックサイズ", &parameters_.blockSize, 0.005f, 0.30f))
	{
		parameters_.particleSize = parameters_.blockSize;
	}
	ImGui::SliderFloat("ブロック回転ランダム", &parameters_.blockRotationRandomness, 0.0f, 8.0f);
	ImGui::Checkbox("表面サンプリング", &parameters_.surfaceSampling);
	ImGui::Checkbox("形状維持", &parameters_.preserveShape);
	ImGui::SliderFloat("寿命", &parameters_.lifeTime, 0.10f, 8.0f);
	ImGui::SliderFloat("拡散力", &parameters_.spreadPower, 0.0f, 8.0f);
	ImGui::SliderFloat("崩れ落ちる力", &parameters_.collapsePower, 0.0f, 8.0f);
	ImGui::SliderFloat("重力", &parameters_.gravity, -10.0f, 10.0f);
	ImGui::SliderFloat("ノイズ強度", &parameters_.noisePower, 0.0f, 8.0f);
	ImGui::SliderFloat("フェード速度", &parameters_.fadeSpeed, 0.1f, 4.0f);
	ImGui::SliderFloat("上方向の力", &parameters_.upwardPower, -4.0f, 8.0f);
	ImGui::SliderFloat("開始遅延", &parameters_.startDelay, 0.0f, 3.0f);
	ImGui::SliderFloat("形状維持時間", &parameters_.shapePreserveTime, 0.0f, 3.0f);
	ImGui::ColorEdit4("基本色", &parameters_.baseColor.x);
	ImGui::SliderFloat("色のばらつき", &parameters_.colorVariation, 0.0f, 0.5f);
	ImGui::Text("デバッグキー: F9でCharacters/body.gltfをプレイヤー位置で再生します。");
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
