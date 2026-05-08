#pragma once
#include "DisintegrationEmitter.h"
#include "DisintegrationRenderer.h"
#include "Matrix4x4.h"

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
		float particleSize = 0.035f;
		float lifeTime = 2.0f;
		float spreadPower = 1.6f;
		float gravity = -0.45f;
		float noisePower = 0.8f;
		float fadeSpeed = 1.0f;
		float upwardPower = 0.65f;
		float startDelay = 0.35f;
		float shapePreserveTime = 0.35f;
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
