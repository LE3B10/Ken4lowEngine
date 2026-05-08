#define NOMINMAX
#include "ReconstructionEmitter.h"

#include <algorithm>
#include <cmath>

std::vector<ReconstructionBlock> ReconstructionEmitter::EmitFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	const Settings& settings)
{
	ModelSurfaceSampler sampler;
	const std::vector<DisintegrationSamplePoint> targets = sampler.SampleFromModel(
		modelData,
		worldMatrix,
		settings.blockCount,
		settings.surfaceSampling,
		settings.blockSize * 1.5f);

	std::vector<ReconstructionBlock> blocks;
	blocks.reserve(targets.size());
	if (targets.empty()) { return blocks; }

	const K4E::Vector3 center = worldMatrix.GetTranslation();
	for (const auto& target : targets)
	{
		const K4E::Vector3 radial = K4E::Vector3::NormalizeSafe(target.position - center, RandomUnitVector());
		K4E::Vector3 scatter = radial * RandomRange(settings.startScatterRadius * 0.35f, settings.startScatterRadius);
		scatter += RandomUnitVector() * RandomRange(0.0f, settings.startScatterRadius * 0.35f);
		scatter.y += RandomRange(-0.25f, settings.startHeight);

		ReconstructionBlock block{};
		block.startPosition = target.position + scatter;
		block.targetPosition = target.position;
		block.position = block.startPosition;
		block.startRotation = {
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
		};
		block.rotation = block.startRotation;
		block.rotationVelocity = {
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
			RandomRange(-settings.rotationRandomness, settings.rotationRandomness),
		};
		block.scale = {
			RandomRange(0.86f, 1.12f),
			RandomRange(0.86f, 1.12f),
			RandomRange(0.86f, 1.12f),
		};
		block.color = MakeColor(settings);
		block.startDelay = RandomRange(0.0f, settings.startDelayRange);
		block.size = settings.blockSize * RandomRange(0.86f, 1.12f);
		block.alpha = 1.0f;
		block.alive = true;
		blocks.push_back(block);
	}

	return blocks;
}

float ReconstructionEmitter::Random01()
{
	return unitDist_(rng_);
}

float ReconstructionEmitter::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 ReconstructionEmitter::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}

K4E::Vector4 ReconstructionEmitter::MakeColor(const Settings& settings)
{
	const float variation = settings.colorVariation;
	const float brightness = RandomRange(1.0f - variation, 1.0f + variation);
	return {
		std::clamp(settings.color.x * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.y * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.z * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.w, 0.0f, 1.0f),
	};
}
