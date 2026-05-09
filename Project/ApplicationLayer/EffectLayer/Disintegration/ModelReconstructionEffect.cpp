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
	settings.placementMode = parameters_.placementMode;
	settings.useRandomScale = parameters_.useRandomScale;
	settings.scaleVariation = parameters_.scaleVariation;
	settings.useRandomRotation = parameters_.useRandomRotation;
	settings.placementSeed = parameters_.placementSeed;
	settings.placementSpacing = parameters_.placementSpacing;
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

std::vector<DisintegrationSamplePoint> ModelReconstructionEffect::GetTargetSamples() const
{
	std::vector<DisintegrationSamplePoint> samples;
	samples.reserve(blocks_.size());
	for (const auto& block : blocks_)
	{
		DisintegrationSamplePoint sample{};
		sample.position = block.targetPosition;
		samples.push_back(sample);
	}
	return samples;
}

void ModelReconstructionEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("モデル再構築エフェクト詳細");
	ImGui::Text("再生中: %s", isActive_ ? "はい" : "いいえ");
	ImGui::Text("完了: %s", isComplete_ ? "はい" : "いいえ");
	ImGui::Text("ブロック数: %zu", blocks_.size());
	ImGui::SliderInt("ブロック数", &parameters_.blockCount, 32, 8000);
	ImGui::SliderFloat("ブロックサイズ", &parameters_.blockSize, 0.005f, 0.30f);
	ImGui::SliderFloat("再構築時間", &parameters_.duration, 0.05f, 8.0f);
	ImGui::SliderFloat("開始時の散らばり半径", &parameters_.startScatterRadius, 0.0f, 12.0f);
	ImGui::SliderFloat("開始高さ", &parameters_.startHeight, -2.0f, 10.0f);
	ImGui::SliderFloat("開始遅延幅", &parameters_.startDelayRange, 0.0f, 3.0f);
	ImGui::Checkbox("ランダム回転を使う", &parameters_.useRandomRotation);
	ImGui::SliderFloat("回転ばらつき", &parameters_.rotationRandomness, 0.0f, 12.0f);
	ImGui::Checkbox("ランダムサイズを使う", &parameters_.useRandomScale);
	ImGui::SliderFloat("サイズばらつき", &parameters_.scaleVariation, 0.0f, 0.75f);
	const char* placementModeLabels[] = { "ランダム表面配置", "均一表面配置", "整列表面配置" };
	int placementModeIndex = parameters_.placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid ? 2 : (parameters_.placementMode == DisintegrationPlacementMode::UniformSurface ? 1 : 0);
	if (ImGui::Combo("配置モード", &placementModeIndex, placementModeLabels, IM_ARRAYSIZE(placementModeLabels)))
	{
		parameters_.placementMode = placementModeIndex == 2 ? DisintegrationPlacementMode::AlignedSurfaceGrid : (placementModeIndex == 1 ? DisintegrationPlacementMode::UniformSurface : DisintegrationPlacementMode::RandomSurface);
		if (parameters_.placementMode != DisintegrationPlacementMode::RandomSurface)
		{
			parameters_.useRandomScale = false;
			parameters_.useRandomRotation = false;
		}
	}
	int placementSeed = static_cast<int>(parameters_.placementSeed);
	if (ImGui::InputInt("配置シード", &placementSeed))
	{
		parameters_.placementSeed = static_cast<uint32_t>(std::max(placementSeed, 0));
	}
	ImGui::SliderFloat("配置間隔", &parameters_.placementSpacing, 0.0f, 0.5f);
	ImGui::SliderFloat("イージング強度", &parameters_.easePower, 1.0f, 8.0f);
	ImGui::ColorEdit4("色", &parameters_.color.x);
	ImGui::SliderFloat("色のばらつき", &parameters_.colorVariation, 0.0f, 0.8f);
	ImGui::SliderFloat("完了後の保持時間", &parameters_.holdTime, 0.0f, 5.0f);
	ImGui::Checkbox("完了後にモデル表示", &parameters_.showFinalModel);
	ImGui::Checkbox("完了後にブロック自動非表示", &parameters_.autoHideBlocksAfterComplete);
	ImGui::Checkbox("表面サンプリング", &parameters_.surfaceSampling);
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
