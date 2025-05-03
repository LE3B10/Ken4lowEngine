#pragma once
#include "IAnimationStrategy.h"

class SkeletonAnimation : public IAnimationStrategy
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~SkeletonAnimation() override = default;

	// アニメーションの状態を初期化
	void Initialize(AnimationModel* model, const std::string& fileName) override;

	// アニメーションの状態を更新
	void Update(AnimationModel* model, float deltaTime) override;

	//
	void Draw(AnimationModel* model) override;
};

