#pragma once
#include "Object3D.h"

#include <memory>

/// -------------------------------------------------------------
///				　			　ステージ基底クラス
/// -------------------------------------------------------------
class Stage
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 描画
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object_; // 地形オブジェクト
};

