#pragma once
#include "Object3D.h"


/// -------------------------------------------------------------
///						スカイドームクラス
/// -------------------------------------------------------------
class Skydome
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object3D_;
};

