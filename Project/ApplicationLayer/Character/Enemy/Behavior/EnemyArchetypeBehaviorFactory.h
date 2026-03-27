#pragma once
#include <memory>
#include "EnemyArchetype.h"

class EnemyArchetypeBehavior;

/// ------------------------------------------------------------
/// EnemyArchetypeBehaviorFactory
/// ------------------------------------------------------------
/// EnemyArchetype から対応する振る舞いクラスを生成する。
/// ------------------------------------------------------------
class EnemyArchetypeBehaviorFactory
{
public:
	static std::unique_ptr<EnemyArchetypeBehavior> Create(EnemyArchetype archetype);
};