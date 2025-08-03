#pragma once
#include "LevelData.h"
#include <Object3D.h>
#include <AnimationModel.h>
#include <memory>
#include <vector>


/// -------------------------------------------------------------
///				レベルオブジェクトマネージャークラス
/// -------------------------------------------------------------
class LevelObjectManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(const LevelData& levelData, const std::string& modelName);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

private: /// ---------- メンバ変数 ---------- ///

	// レベルデータから生成されたオブジェクトのリスト
	std::vector<std::unique_ptr<Object3D>> objects_;
	std::vector<std::unique_ptr<AnimationModel>> animationModels_; // アニメーションモデル用のリスト
};
