#pragma once
#include <string>
#include <memory>
#include <unordered_map>

#include "AnimationModel.h"

/// -------------------------------------------------------------
///				　アニメーションモデルファクトリ
/// -------------------------------------------------------------
class AnimationModelFactory
{
public: /// ---------- メンバ関数 ---------- ///

	~AnimationModelFactory();

	// アニメーションモデルの生成
	static void PreLoadModel(const std::string& modelName);

	static std::shared_ptr<AnimationModel> CreateInstance(const std::string& modelName);

	// 全キャッシュをクリアする
	static void ClearAll();

private: /// ---------- メンバ変数 ---------- ///

	static std::unordered_map<std::string, std::shared_ptr<AnimationModel>> modelCache_;
};

