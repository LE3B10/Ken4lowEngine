#pragma once
#include <Vector4.h>

/// ---------- マズルフラッシュ設定 ---------- ///
struct MuzzleData
{
	bool  enabled = true;					  // マズルフラッシュ有効
	float life = 0.06f;						  // 寿命（秒）
	float startLength = 0.2f;				  // 初期の長さ [m]
	float endLength = 0.05f;				  // 終了時の長さ（しぼむ） [m]
	float startWidth = 0.1f;				  // 初期の太さ [m]
	float endWidth = 0.03f;					  // 終了時の太さ [m]
	float randomYawDeg = 0.0f;				  // 発射ごとのランダム広がり（度）
	Vector4 color = { 1.2f,1.0f,0.6f,1.0f };  // 色 (RGBA)

	float offsetForward = 0.0f;		  // フラッシュ根元を前後にオフセット
	bool  sparksEnabled = true;		  // 火花を出すか
	int   sparkCount = 14;			  // 1発で何本
	float sparkLifeMin = 0.08f;		  // 秒 : 火花の最小寿命
	float sparkLifeMax = 0.14f;		  // 秒 : 火花の最大寿命
	float sparkSpeedMin = 8.0f;		  // m/s : 火花の最小速度
	float sparkSpeedMax = 16.0f;	  // m/s : 火花の最大速度
	float sparkConeDeg = 28.0f;		  // 前方への拡がり角（円錐）
	float sparkGravityY = -25.0f;	  // 火花用重力（強めに落とす）
	float sparkWidth = 0.018f;		  // 太さ
	float sparkOffsetForward = 0.02f; // 火花の開始位置

	// RGBA
	Vector4 sparkColorStart = { 1.0f,1.0f,0.6f,1.0f }; // 明るい橙
	Vector4 sparkColorEnd = { 0.8f,0.2f,0.0f,0.0f };   // 赤～消失
};