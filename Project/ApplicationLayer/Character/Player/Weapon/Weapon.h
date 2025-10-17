#pragma once
#include "Object3D.h"
#include "WorldTransformEx.h"

#include <memory>

/// -------------------------------------------------------------
///					　		武器クラス
/// -------------------------------------------------------------
class Weapon
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- アクセサー関数 ---------- ///

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object_; // 武器モデル

};

