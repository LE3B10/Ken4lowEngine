#pragma once

#include "EnemyType.h"

#include <memory>

class EnemyBase;

/// 通常ゲーム用の雑魚敵を生成し、Legacy Enemyの置き換え境界を集約する。
class EnemyFactory
{
public:
	static std::unique_ptr<EnemyBase> Create(EnemyType enemyType);
};
