#pragma once
#include <random>
#include "Vector3.h"
#include "Particle.h"
#include "ParticleEffectType.h"

/// -------------------------------------------------------------
///						パーティクル生成クラス
/// -------------------------------------------------------------
class ParticleFactory
{
public: /// ---------- メンバ関数 ---------- ///

	// パーティクルを生成する関数
	static Particle Create(std::mt19937& randomEngine, const Vector3& position, ParticleEffectType effectType);

	// レーザービームパーティクルを生成する関数
	static Particle CreateLaserBeam(const Vector3& position, float length, const Vector3& color);
};

