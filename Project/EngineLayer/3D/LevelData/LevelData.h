#pragma once
#include <string>
#include <vector>
#include <Vector3.h>

struct ObjectData
{
	Vector3 position; // 位置
	Vector3 rotation; // 回転
	Vector3 scale;    // スケール
	std::string fileName; // モデルファイル名
};

struct LevelData
{
	std::vector<ObjectData> objects; // オブジェクトのリスト
};
