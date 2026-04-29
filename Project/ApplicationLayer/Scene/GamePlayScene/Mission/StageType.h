#pragma once

enum class StageType
{
	Wave,
	Explore,
	Defense,
	Escape,
	Boss
};

StageType ResolveStageTypeFromStageIndex(int stageIndex);
const char* ToStageTypeName(StageType stageType);
