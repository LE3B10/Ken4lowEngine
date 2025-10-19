#pragma once
#include <Vector4.h>

// マズルフラッシュ設定
struct MuzzleFlashSettings
{
	bool    enabled = true;
	float   life = 0.06f;        // 60fpsで ~3～4フレ
	float   startLength = 0.20f;        // [m] 初期の長さ
	float   endLength = 0.05f;        // [m] 終了時の長さ（しぼむ）
	float   startWidth = 0.10f;        // [m] 初期の太さ
	float   endWidth = 0.03f;        // [m] 終了時の太さ
	float   randomYawDeg = 0.0f;         // 発射毎のランダム広がり
	Vector4 color = { 1.0f, 1.0f, 0.6f, 1.0f }; // 温かい発光色（αはコード側でフェード）

	float   offsetForward = 0.00f; // フラッシュ根元を前後
	bool  sparksEnabled = true;     // 火花を出すか
	int   sparkCount = 18;       // 1発で何本
	float sparkLifeMin = 0.08f;    // 秒
	float sparkLifeMax = 0.16f;    // 秒
	float sparkSpeedMin = 10.0f;    // m/s
	float sparkSpeedMax = 22.0f;    // m/s
	float sparkConeDeg = 30.0f;    // 前方への拡がり角（円錐）
	float sparkGravityY = -30.0f;   // 火花用重力（強めに落とす）
	float sparkWidth = 0.018f;   // 太さ
	float sparkOffsetForward = 0.02f; // 火花の開始位置
	Vector4 sparkColorStart = { 1.0f,1.0f,0.6f,1.0f }; // 明るい橙
	Vector4 sparkColorEnd = { 0.8f,0.2f,0.0f,0.0f }; // 赤～消失
};