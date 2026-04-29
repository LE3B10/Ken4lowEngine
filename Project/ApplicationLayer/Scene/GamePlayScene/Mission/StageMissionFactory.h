#pragma once

#include "StageType.h"
#include <memory>

class IStageMission;

std::unique_ptr<IStageMission> CreateStageMissionByType(StageType stageType);
