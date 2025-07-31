#pragma once
#include <string>
#include <vector>
#include <Vector3.h>

/// -------------------------------------------------------------
///				　		オブジェクトデータ構造体
/// -------------------------------------------------------------
struct ObjectData
{
	std::string name; // オブジェクトの名前
	Vector3 position; // 位置
	Vector3 rotation; // 回転
	Vector3 scale; // スケール
};

/// -------------------------------------------------------------
///				　		レベルデータ構造体
/// -------------------------------------------------------------
struct LevelData
{
	std::vector<ObjectData> objects; // レベル内のオブジェクトデータ
};