#pragma once
#include "Camera.h"
#include "Object3D.h"

class Object3DCommon;

/// -------------------------------------------------------------
///							プレイヤークラス
/// -------------------------------------------------------------
class Player
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();


private: /// ---------- メンバ変数 ---------- ///

	// オブジェクト
	std::unique_ptr<Object3D> object3D_;

};
