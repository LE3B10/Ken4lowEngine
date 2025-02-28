#pragma once
#include "BaseCharacter.h"


/// -------------------------------------------------------------
///							　敵クラス
/// -------------------------------------------------------------
class Enemy : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

private: /// ---------- メンバ関数 ---------- ///

	// 腕のアニメーションを更新する処理
	void UpdateArmAnimation();

private: /// ---------- メンバ変数 ---------- ///

	float angle_ = 0.0f;
	float radius_ = 100.0f;
	float speed_ = 0.005f;

	// 腕のアニメーション用の媒介変数
	float armSwingParameter_ = 0.0f;

	// 腕の振りの最大角度（ラジアン）
	const float kMaxArmSwingAngle = 0.3f;

	// 腕の振りの速度
	const float kArmSwingSpeed = 0.1f;
};

