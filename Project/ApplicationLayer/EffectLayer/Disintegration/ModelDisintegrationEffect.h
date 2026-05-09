#pragma once
#include "DisintegrationEmitter.h"
#include "DisintegrationRenderer.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include "Vector3.h"

#include <string>
#include <vector>

/// -------------------------------------------------------------
/// モデル形状を小さいブロック粒子として崩して消す完全新規エフェクト
/// -------------------------------------------------------------
class ModelDisintegrationEffect
{
public:
	struct Parameters
	{
		int particleCount = 1200;
		bool blockMode = true;
		float particleSize = 0.045f;
		float blockSize = 0.045f;
		float blockRotationRandomness = 1.2f;
		bool surfaceSampling = true;
		bool preserveShape = true;
		float lifeTime = 2.2f;
		float spreadPower = 1.6f;
		float collapsePower = 1.0f;
		float gravity = -1.25f;
		float noisePower = 0.55f;
		float fadeSpeed = 1.0f;
		float upwardPower = 0.15f;
		float startDelay = 0.15f;
		float shapePreserveTime = 0.60f;
		K4E::Vector4 baseColor{ 0.64f, 0.61f, 0.56f, 1.0f };
		float colorVariation = 0.16f;
		bool useSweepErosion = false;
		K4E::Vector3 sweepDirection{ 1.0f, 0.0f, 0.0f };
		float sweepDuration = 1.6f;
		float erosionNoisePower = 0.35f;
		float erosionBandWidth = 0.10f;
		float preGlowWidth = 0.20f;
		float postGlowWidth = 0.16f;
		float glowEdgeWidth = 0.24f;
		float glowIntensity = 1.8f;
		float glowSharpness = 1.6f;
		K4E::Vector4 glowColor{ 1.0f, 0.55f, 0.18f, 1.0f };
	};

	void Initialize();
	void PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void Update(float deltaTime);
	void Draw();
	void DrawImGui();
	bool IsActive() const { return isActive_; }

	Parameters& GetParameters() { return parameters_; }
	const Parameters& GetParameters() const { return parameters_; }

private:
	K4E::Vector3 RandomNoiseVector();
	K4E::Vector3 GetSafeSweepDirection() const;
	float ErosionNoise(const K4E::Vector3& origin) const;
	void InitializeSweepData();
	void UpdateSweepErosion();
	void UpdateParticlePhysics(DisintegrationParticle& particle, float deltaTime);
	void StopIfFinished();

	Parameters parameters_{};
	DisintegrationEmitter emitter_{};
	DisintegrationRenderer renderer_{};
	std::vector<DisintegrationParticle> particles_{};
	bool isActive_ = false;
	float elapsedTime_ = 0.0f;
	K4E::Vector3 sweepDirectionNormalized_{ 1.0f, 0.0f, 0.0f };
	float sweepMin_ = 0.0f;
	float sweepMax_ = 0.0f;
};
