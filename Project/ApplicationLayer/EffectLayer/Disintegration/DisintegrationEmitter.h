#pragma once
#include "DisintegrationParticle.h"
#include "Matrix4x4.h"
#include "ModelData.h"
#include "Vector4.h"

#include <random>
#include <vector>

/// -------------------------------------------------------------
/// モデル表面から崩壊粒子の初期配置を作る専用エミッター
/// -------------------------------------------------------------
class DisintegrationEmitter
{
public:
	struct Settings
	{
		int particleCount = 1200;
		float particleSize = 0.018f;
		float lifeTime = 2.0f;
		float spreadPower = 1.6f;
		float upwardPower = 0.65f;
		float startDelay = 0.20f;
		K4E::Vector4 baseColor{ 0.64f, 0.61f, 0.56f, 1.0f };
		float colorVariation = 0.16f;
	};

	std::vector<DisintegrationParticle> EmitFromModel(
		const K4E::ModelData& modelData,
		const K4E::Matrix4x4& worldMatrix,
		const Settings& settings);

private:
	struct TriangleSample
	{
		K4E::Vector3 a{};
		K4E::Vector3 b{};
		K4E::Vector3 c{};
		K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
		float cumulativeArea = 0.0f;
	};

	struct VertexSample
	{
		K4E::Vector3 position{};
		K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
	};

	float Random01();
	float RandomRange(float minValue, float maxValue);
	K4E::Vector3 RandomUnitVector();

	std::mt19937 rng_{ 0xD157E6A7u };
	std::uniform_real_distribution<float> unitDist_{ 0.0f, 1.0f };
};
