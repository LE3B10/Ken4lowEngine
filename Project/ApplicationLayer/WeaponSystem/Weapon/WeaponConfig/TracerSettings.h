#pragma once
#include <Vector4.h>

// 弾道（トレーサ）見た目の設定
struct TracerSettings
{
	bool   enabled = true;        // トレーサ有効
	float  tracerLength = 10.0f;  // 見た目の長さ [m]
	float  tracerWidth = 0.02f;  // 幅 [m]
	float  minSegLength = 0.05f;  // セグメント間引き閾値
	float  startOffsetForward = 0.6f; // [m] 弾/トレーサの開始点を銃口から前後にオフセット
	int    tracerInterval = 1;    // 1 = every shot, 2 = every 2nd shot, ...
	Vector4 tracerColor = { 0.8f,1.0f,0.6f,1.0f }; // RGBA
};