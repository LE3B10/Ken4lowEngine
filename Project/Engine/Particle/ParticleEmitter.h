#pragma once
#include "DX12Include.h"
#include "AABB.h"
#include "Emitter.h"
#include "Particle.h"
#include "ParticleEmitter.h"
#include "ParticleForGPU.h"

#include <list>
#include <numbers>
#include <random>

/// ---------- 風のエフェクト ---------- ///
struct WindZone
{
	AABB area;		  // 風が吹くエリア
	Vector3 strength; // 風の強さ
};


/// -------------------------------------------------------------
///				パーティクルを発生させるクラス
/// -------------------------------------------------------------
class ParticleEmitter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// パーティクル生成関数
	Particle MakeNewParticle(std::mt19937& randomEngine, const Vector3& translate);

	// パーティクルを射出する関数
	std::list<Particle> Emit(const Emitter& emitter, std::mt19937& randomEngine);

	// 当たり判定
	bool IsCollision(const AABB& aabb, const Vector3& point);

private: /// ---------- メンバ変数 ---------- ///


};

