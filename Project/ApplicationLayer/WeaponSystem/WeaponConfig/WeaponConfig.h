#pragma once
#include "TracerSettings.h"
#include "MuzzleFlashSettings.h"
#include "CasingSettings.h"
#include <string>
#include <cstdint>

// 武器ごとの設定
struct WeaponConfig
{
	std::string name; 		   // 武器名
	float       muzzleSpeed = 252.0f; // m/s (銃口初速)
	float       maxDistance = 200.0f; // m (弾の飛距離)
	float       rpm = 600.0f;         // 発射レート (rounds per minute)
	uint32_t   magCapacity = 15;    // 1マガジンの装弾数
	uint32_t   startingReserve = 60;   // 初期予備弾数（総予備）
	float reloadTime = 1.6f;  // リロード時間[秒]
	uint32_t   bulletsPerShot = 1;     // 1発で何弾出すか（SGなら>1）
	bool  autoReload = true;  // 弾切れ時に自動リロードするか
	uint32_t requestedMaxSegments = 256; // この武器が想定する同時セグメント上限

	float spreadDeg = 1.5f; // 発射時の拡がり角（度）
	int pelletTracerMode = 0; // 散弾のトレーサ表示モード (0=無し, 1=1発のみ, 2=散弾)
	int pelletTracerCount = 1; // 散弾のトレーサ表示数（pelletTracerMode=2時）

	TracerSettings tracer; 		// トレーサ設定
	MuzzleFlashSettings muzzle; // マズルフラッシュ設定
	CasingSettings casing;  // 薬莢設定
};