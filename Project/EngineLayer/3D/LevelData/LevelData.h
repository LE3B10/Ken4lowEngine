#pragma once
#include <string>
#include <vector>
#include <Vector3.h>

/// -------------------------------------------------------------
///				　	ボックスコライダーデータ構造体
/// -------------------------------------------------------------
struct ObjectColliderData
{
	std::string type; // コライダーの名前
	Vector3 center; // 中心位置
	Vector3 size;   // サイズ
	bool enabled = false; // 有効フラグ
};

/// -------------------------------------------------------------
///				　		オブジェクトデータ構造体
/// -------------------------------------------------------------
struct ObjectData
{
	std::string name; // オブジェクトの名前
	std::string type; // オブジェクトのタイプ（例: "Player", "Enemy", "Item"など）
	Vector3 position; // 位置
	Vector3 rotation; // 回転
	Vector3 scale;	  // スケール
	ObjectColliderData collider; // コライダーデータ
};

/// -------------------------------------------------------------
///				　		レベルデータ構造体
/// -------------------------------------------------------------
struct LevelData
{
	std::vector<ObjectData> objects; // レベル内のオブジェクトデータ
};