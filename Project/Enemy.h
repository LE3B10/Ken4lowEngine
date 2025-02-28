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

private: /// ---------- メンバ変数 ---------- ///

	float angle_ = 0.0f;
	float radius_ = 100.0f;
	float speed_ = 0.005f;
};

