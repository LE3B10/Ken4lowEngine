#pragma once
#include "Collider.h"

/// -------------------------------------------------------------
///					　キャラクター基底クラス
/// -------------------------------------------------------------
class BaseCharacter : public Collider
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~BaseCharacter() = default;

	// 初期化処理
	virtual void Initialize() = 0;

	// 更新処理
	virtual void Update() = 0;

	// 描画処理
	virtual void Draw() = 0;

	// ImGui描画処理
	virtual void DrawImGui() = 0;

	// 衝突判定を行う
	virtual void OnCollision(Collider* other) override = 0;
};

