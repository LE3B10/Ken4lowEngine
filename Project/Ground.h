#pragma once
#include "Object3D.h"

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;


/// -------------------------------------------------------------
///				　			　 地面クラス
/// -------------------------------------------------------------
class Ground
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> object3D_;

};

