#pragma once
#include "DisintegrationEmitter.h"
#include "DisintegrationRenderer.h"
#include "Matrix4x4.h"
#include "Vector4.h"

#include <string>
#include <vector>

/// -------------------------------------------------------------
/// モデル形状を砂・灰・霧状に分解して消す完全新規エフェクト
/// -------------------------------------------------------------
class ModelDisintegrationEffect
{
public:
	struct Parameters
	{
		int particleCount = 1200;
		float particleSize = 0.018f;
		float lifeTime = 2.2f;
		float spreadPower = 1.6f;
		float gravity = -1.25f;
		float noisePower = 0.55f;
		float fadeSpeed = 1.0f;
		float upwardPower = 0.15f;
		float startDelay = 0.15f;
		float shapePreserveTime = 0.60f;
		K4E::Vector4 baseColor{ 0.64f, 0.61f, 0.56f, 1.0f };
		float colorVariation = 0.16f;
	};

	void Initialize();
	void PlayFromModel(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix);
	void Update(float deltaTime);
	void Draw() const;
	void DrawImGui();
	bool IsActive() const { return isActive_; }

	Parameters& GetParameters() { return parameters_; }
	const Parameters& GetParameters() const { return parameters_; }

private:
	K4E::Vector3 RandomNoiseVector();
	void StopIfFinished();

	Parameters parameters_{};
	DisintegrationEmitter emitter_{};
	DisintegrationRenderer renderer_{};
	std::vector<DisintegrationParticle> particles_{};
	bool isActive_ = false;
	float elapsedTime_ = 0.0f;
};
