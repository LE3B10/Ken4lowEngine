#pragma once
#include "LevelData.h"
#include <memory>

class LevelLoader
{
public:
	static std::unique_ptr<LevelData> Load(const std::string& filepath);
};

