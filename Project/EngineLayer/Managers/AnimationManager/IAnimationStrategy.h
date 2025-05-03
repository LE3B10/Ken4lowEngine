#pragma once
#include <string>

class AnimationModel;

/// -------------------------------------------------------------
///		　アニメーションの状態を管理するインターフェース
/// -------------------------------------------------------------
class IAnimationStrategy
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~IAnimationStrategy() = default;

	// アニメーションの状態を初期化
	virtual void Initialize(AnimationModel* model, const std::string& fileName) = 0;

	// アニメーションの状態を更新
	virtual void Update(AnimationModel* model, float deltaTime) = 0;

	// 描画処理
	virtual void Draw(AnimationModel* model) = 0;
};
