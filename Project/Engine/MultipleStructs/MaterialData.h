#pragma once
#include "d3d12.h"
#include <string>

// MaterialDataの構造体
struct MaterialData
{
	std::string textureFilePath; // テクスチャファイルパス
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
};
