#define NOMINMAX
#include "ModelReconstructionEffect.h"

#include "Model.h"
#include "ModelManager.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void ModelReconstructionEffect::Initialize()
{
	blocks_.clear();
	isActive_ = false;
	isComplete_ = false;
	elapsedTime_ = 0.0f;
}

void ModelReconstructionEffect::PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	auto model = K4E::ModelManager::GetInstance()->LoadModel(modelPath);
	if (!model) { return; }

	ReconstructionEmitter::Settings settings{};
	settings.blockCount = parameters_.blockCount;
	settings.blockSize = parameters_.blockSize;
	settings.startScatterRadius = parameters_.startScatterRadius;
	settings.startHeight = parameters_.startHeight;
	settings.startDelayRange = parameters_.startDelayRange;
	settings.rotationRandomness = parameters_.rotationRandomness;
	settings.surfaceSampling = parameters_.surfaceSampling;
	settings.color = parameters_.color;
	settings.colorVariation = parameters_.colorVariation;

	blocks_ = emitter_.EmitFromModel(model->GetModelData(), worldMatrix, settings);
	isActive_ = !blocks_.empty();
	isComplete_ = false;
	elapsedTime_ = 0.0f;
}

void ModelReconstructionEffect::Update(float deltaTime)
{
	if (!isActive_ && parameters_.autoHideBlocksAfterComplete) { return; }
	if (blocks_.empty()) { return; }

	elapsedTime_ += deltaTime;
	bool allArrived = true;

	for (auto& block : blocks_)
	{
		if (!block.alive) { continue; }

		block.age += deltaTime;
		if (block.age < block.startDelay)
		{
			block.position = block.startPosition;
			allArrived = false;
			continue;
		}

		const float localT = Clamp01((block.age - block.startDelay) / std::max(parameters_.duration, 0.0001f));
		const float easedT = EaseOut(localT);

		// 到着直前で回転速度を弱め、最終的には表面サンプル位置に静止させる。
		block.position = block.startPosition + (block.targetPosition - block.startPosition) * easedT;
		block.rotation = block.startRotation + block.rotationVelocity * ((1.0f - easedT) * block.age);
		block.arrived = localT >= 1.0f;
		if (block.arrived)
		{
			block.position = block.targetPosition;
			block.rotation = { 0.0f, 0.0f, 0.0f };
		}
		else
		{
			allArrived = false;
		}
	}

	const float totalTime = parameters_.startDelayRange + parameters_.duration + parameters_.holdTime;
	if (allArrived && elapsedTime_ >= totalTime)
	{
		isComplete_ = true;
	}

	if (isComplete_ && parameters_.autoHideBlocksAfterComplete)
	{
		isActive_ = false;
		for (auto& block : blocks_)
		{
			block.alive = false;
		}
	}
}

void ModelReconstructionEffect::Draw()
{
	if (!ShouldDrawBlocks()) { return; }
	renderer_.Draw(blocks_);
}

void ModelReconstructionEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Model Reconstruction Effect");
	ImGui::Text("Active: %s", isActive_ ? "true" : "false");
	ImGui::Text("Complete: %s", isComplete_ ? "true" : "false");
	ImGui::Text("Block Instances: %zu", blocks_.size());
	ImGui::SliderInt("blockCount", &parameters_.blockCount, 32, 8000);
	ImGui::SliderFloat("blockSize", &parameters_.blockSize, 0.005f, 0.30f);
	ImGui::SliderFloat("duration", &parameters_.duration, 0.05f, 8.0f);
	ImGui::SliderFloat("startScatterRadius", &parameters_.startScatterRadius, 0.0f, 12.0f);
	ImGui::SliderFloat("startHeight", &parameters_.startHeight, -2.0f, 10.0f);
	ImGui::SliderFloat("startDelayRange", &parameters_.startDelayRange, 0.0f, 3.0f);
	ImGui::SliderFloat("rotationRandomness", &parameters_.rotationRandomness, 0.0f, 12.0f);
	ImGui::SliderFloat("easePower", &parameters_.easePower, 1.0f, 8.0f);
	ImGui::ColorEdit4("color", &parameters_.color.x);
	ImGui::SliderFloat("colorVariation", &parameters_.colorVariation, 0.0f, 0.8f);
	ImGui::SliderFloat("holdTime", &parameters_.holdTime, 0.0f, 5.0f);
	ImGui::Checkbox("showFinalModel", &parameters_.showFinalModel);
	ImGui::Checkbox("autoHideBlocksAfterComplete", &parameters_.autoHideBlocksAfterComplete);
	ImGui::Checkbox("surfaceSampling", &parameters_.surfaceSampling);
	ImGui::End();
#endif
}

float ModelReconstructionEffect::Clamp01(float value) const
{
	return std::clamp(value, 0.0f, 1.0f);
}

float ModelReconstructionEffect::EaseOut(float value) const
{
	const float t = Clamp01(value);
	return 1.0f - std::pow(1.0f - t, std::max(parameters_.easePower, 1.0f));
}
