#pragma once
#include <string>

// MaterialDataの構造体
struct MaterialData
{
	std::string textureFilePath; // テクスチャファイルパス
	uint32_t textureIndex = 0;	 // テクスチャ番号
};
