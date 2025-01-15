#pragma once
#include <vector>
#include "VertexData.h"
#include "MaterialData.h"
#include <Matrix4x4.h>

struct Node
{
	Matrix4x4 localMatrix;
	std::string name;
	std::vector<Node> children;
};

// ModelData構造体
struct ModelData
{
	std::vector<VertexData> vertices;
	MaterialData material;
	Node rootNode;
};

