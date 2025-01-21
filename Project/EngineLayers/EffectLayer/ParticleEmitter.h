#pragma once
#include "DX12Include.h"
#include "AABB.h"
#include "ParticleManager.h"


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
	void Initialize(std::string name);

	// 更新処理
	void Update();

	// パーティクルを射出する関数
	void Emit();

private: /// ---------- メンバ変数 ---------- ///

	Transform transform;

	Emitter emitter{};

	std::string name_;

	std::unordered_map<std::string, ParticleManager::ParticleGroup> particleGroups;
};

