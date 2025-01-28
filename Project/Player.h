#pragma once
#include "WorldTransform.h"
#include "Camera.h"
#include "Object3D.h"

#include <vector>
#include <memory>

/// ---------- 前方宣言 ---------- ///
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

private: /// ---------- 構造体 ---------- ///

	struct Part
	{
		WorldTransform worldTransform;
		std::shared_ptr<Object3D> object3D; // shared_ptrでコピー可能
		std::string modelFile;
	};

private: /// ---------- メンバ変数 ---------- ///

	std::vector<Part> parts_; // 各部位をまとめて管理

};
