#pragma once
#include <Vector4.h>

namespace K4E = ::Ken4lowEngine;

/// ---------- 弾道（トレーサ）見た目の設定 ---------- ///
struct TracerData
{
	bool   enabled = true;            // トレーサ有効
	float  tracerLength = 5.0f;       // [m] 見た目の長さ
	float  tracerWidth = 0.02f;       // [m] 幅
	float  minSegLength = 0.04f;      // セグメント間引き閾値
	float  startOffsetForward = 0.0f; // [m] 弾/トレーサの開始点を銃口から前後にオフセット
	// RGBA
	K4E::Vector4  color = { 0.8f, 1.0f, 0.6f, 1.0f };
};