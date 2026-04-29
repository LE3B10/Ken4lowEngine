#include "StageType.h"

StageType ResolveStageTypeFromStageIndex(int stageIndex)
{
	switch (stageIndex)
	{
	case 0: return StageType::Wave;
	case 1: return StageType::Explore;
	case 2: return StageType::Defense;
	case 3: return StageType::Escape;
	case 4: return StageType::Boss;
	default: return StageType::Wave;
	}
}

const char* ToStageTypeName(StageType stageType)
{
	switch (stageType)
	{
	case StageType::Wave: return "Wave";
	case StageType::Explore: return "Explore";
	case StageType::Defense: return "Defense";
	case StageType::Escape: return "Escape";
	case StageType::Boss: return "Boss";
	default: return "Unknown";
	}
}
