#pragma once
#include "CasingData.h"
#include "MuzzleData.h"
#include "TracerData.h"

#include <string>

/// ---------- 武器設定構造体 ---------- ///
struct WeaponData
{
	std::string name;           // 武器名
	float muzzleSpeed = 252.0f; // m/s (銃口初速)
	float maxDistance = 200.0f; // m (弾の飛距離)
	float rpm = 300.0f;         // 発射レート (rounds per minute)

	// magazine
	int   magCapacity = 15;		// 1マガジンの装弾数
	int   startingReserve = 60; // 初期予備弾数（総予備）
	float reloadTime = 1.5f;	// リロード時間[秒]
	int   bulletsPerShot = 1;	// 1発で何弾出すか（SGなら>1）
	bool  autoReload = true;	// 弾切れ時に自動リロードするか

	// 弾道（トレーサ）セグメント最大数
	int requestedMaxSegments = 256;

	float spreadDeg = 0.0f; // 発射時の拡がり角（度）
	int  pelletTracerMode = 0; // 散弾時のトレーサ動作モード（0=なし,1=1発,2=全発）
	int  pelletTracerCount = 1; // 散弾時にトレーサを出す発数（pelletTracerMode==1時のみ有効）

	TracerData tracer; // トレーサ設定
	MuzzleData muzzle; // マズルフラッシュ設定
	CasingData casing; // 薬莢設定
};
