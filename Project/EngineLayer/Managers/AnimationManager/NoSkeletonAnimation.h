#pragma once
#include "IAnimationStrategy.h"

class NoSkeletonAnimation : public IAnimationStrategy
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~NoSkeletonAnimation() override = default;

	// アニメーションの状態を初期化
	void Initialize(AnimationModel* model, const std::string& fileName) override;

	// アニメーションの状態を更新
	void Update(AnimationModel* model, float deltaTime) override;
};

