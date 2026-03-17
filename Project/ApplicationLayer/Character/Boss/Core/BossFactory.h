#pragma once
#include "BossBase.h"

#include <memory>

enum class BossType
{
    ForestGuardian,
    FlameBeast,
    SandWorm,
    MachineCore,
    IceQueen
};

class BossFactory
{
public:
    static std::unique_ptr<BossBase> Create(BossType type);
};
