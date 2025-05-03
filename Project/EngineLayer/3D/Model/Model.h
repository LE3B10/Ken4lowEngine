#pragma once
#include "Object3D.h"

#include <string>

/// ---------- 前方宣言 ---------- ///


/// -------------------------------------------------------------
///						　3Dモデルクラス
/// -------------------------------------------------------------
class Model
{
public: /// ---------- メンバ関数 ---------- ///

	//　初期化処理
	void Initialize(const std::string& fileName);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	// オブジェクト3Dデータ
	std::unique_ptr<Object3D> object3D_;
};

