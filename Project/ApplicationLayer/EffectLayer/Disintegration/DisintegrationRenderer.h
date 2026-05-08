#pragma once
#include "DisintegrationParticle.h"

#include <vector>

/// -------------------------------------------------------------
/// 既存Particleに依存しない崩壊エフェクト専用レンダラー
/// -------------------------------------------------------------
class DisintegrationRenderer
{
public:
	void Draw(const std::vector<DisintegrationParticle>& particles) const;
};
