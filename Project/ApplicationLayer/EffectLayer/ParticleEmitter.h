#pragma once
#include "DX12Include.h"
#include "AABB.h"

/// ---------- 前方宣言 ---------- ///
class ParticleManager;


/// -------------------------------------------------------------
///				パーティクルを発生させるクラス
/// -------------------------------------------------------------
class ParticleEmitter
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	ParticleEmitter(ParticleManager* manager, const std::string& groupName);

	// 更新処理
	void Update();

	// 座標を設定する関数
	void SetPosition(const Vector3& position) { position_ = position; }

	// 1秒あたりの射出数
	void SetEmissionRate(float rate) { emissionRate_ = rate; }

	// 色や寿命などの設定
	void SetParticleAttributes() {}
	
	// 座標を取得
	Vector3 GetPosition() const { return position_; }

	// 射出数を取得
	float GetEmissionRate() const { return emissionRate_; }

private: /// ---------- メンバ変数 ---------- ///
	
	ParticleManager* particleManager_; // パーティクルマネージャへの参照
	std::string groupName_;            // 射出先のパーティクルグループ名
	Vector3 position_;                 // 射出位置
	float emissionRate_;               // 射出レート (1秒あたりのパーティクル数)
	float accumulatedTime_;            // 射出タイミング計算用
};

