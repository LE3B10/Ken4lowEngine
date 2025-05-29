#pragma once
#include "ParticleTransform.h"	
#include "Vector3.h"
#include "Vector4.h"

// パーティクルのエフェクトタイプ
enum class ParticleMode
{
	Orbit,     // 回転（チャージ中）
	Explode    // 爆発（チャージ解除後）
};

// パーティクル用の構造体
struct Particle
{
	ParticleTransform transform{};	 // 位置
	Vector3 velocity = {};	 // 速度
	Vector4 color = {};		 // 色
	float lifeTime = 0;		 // 生存可能な時間
	float currentTime = 0;	 // 発生してからの経過時間

	// スケールアニメーション用（追加）
	Vector3 startScale = { 1.0f, 1.0f, 1.0f };
	Vector3 endScale = { 0.0f, 0.0f, 0.0f };

	Vector3 orbitCenter{};         // 回転の中心
	Vector3 orbitAxis{};           // 回転軸（Normalize済）
	float orbitRadius = 1.0f;      // 回転半径
	float orbitSpeed = 1.0f;       // 速度（時間の倍率）
	float orbitPhase = 0.0f;       // 初期角度

	ParticleMode mode = ParticleMode::Orbit;
};
