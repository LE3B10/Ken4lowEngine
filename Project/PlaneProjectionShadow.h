#pragma once
#include "WorldTransform.h"
#include "Object3D.h"


/// -------------------------------------------------------------
///				　	平面投影行列による影のクラス
/// -------------------------------------------------------------
class PlaneProjectionShadow
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ変数 ---------- //

	// 影自体のワールド変換データ
	WorldTransform worldTransform_;

	// 投影元オブジェクトのワールド変換データ
	WorldTransform* castaerWorldTransform_ = nullptr;

	// オブジェクトモデル
	std::unique_ptr<Object3D> object3D_;

};

