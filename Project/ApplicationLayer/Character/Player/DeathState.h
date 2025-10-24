#pragma once
#include "Vector3.h"

// 死亡モード
enum class DeathMode
{
	BlowAway, 	// 後方に吹っ飛ぶ
};

// 死亡状態構造体
struct DeathState
{
	DeathMode mode = DeathMode::BlowAway; // 死亡モード
	bool isDead = false;          // 死亡フラグ
	bool inDeathSeq = false;    // 死亡演出中フラグ
	float timer = 0.0f;         // 死亡演出タイマー
	float length = 3.0f;        // 死亡演出時間
	int side = 1;               // 横倒れ方向(+1 or -1)

	Vector3 cameraStartPos{ 0.0f, 0.0f, 0.0f }; // カメラ奪取用開始位置
	Vector3 cameraEndOffset{ 0.0f, 2.0f, -20.0f }; // カメラ奪取用終了オフセット
	Vector3 camLockPos{ 0,0,0 };
	Vector3 camLockRot{ 0,0,0 }; // {pitch,yaw,roll}
	bool   camLock = true;      // 死亡中はカメラ固定するか

	Vector3 velocity{ 0,0,0 };      // 並進速度（m/s）
	Vector3 angularVelocity{ 0,0,0 };   // 角速度（rad/s）(x=pitch, y=yaw, z=roll)
	float bounce = 0.25f;          // 地面反発係数
	float friction = 0.7f;        // 地面摩擦係数

	// 抵抗係数
	float linDragK = 1.0f; // 並進抵抗
	float quadDragK = 0.1f; // 二次抵抗
	float angLink = 0.5f; // 角速度抵抗
	float angQuadK = 0.1f; // 角速度二次抵抗
};