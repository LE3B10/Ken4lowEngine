#pragma once

#include <cstdint>

/// -------------------------------------------------------------
///  WeaponParams
///  - マスターデータ(FWeaponMasterData)から生成する「固定パラメータ」
///  - ゲーム中に変化する値（残弾/クールダウン等）は WeaponRuntimeState に持つ
/// -------------------------------------------------------------
struct WeaponParams
{
	// --- Identity ---
	int32_t weaponID = 0;

	// --- Basic ---
	bool    isAutomatic = false;
	bool    canToggleFireMode = true;

	// --- Damage / Fire ---
	float   damage = 10.0f;
	float   secPerShot = 0.1f;

	int32_t magCapacity = 30;
	int32_t ammoPerShot = 1;
	int32_t maxReserveAmmo = 90;

	// リロード（共通 + 詳細）
	float   reloadSec = 1.6f;            // フォールバック
	float   tacticalReloadSec = 1.6f;    // 残弾あり
	float   emptyReloadSec = 2.0f;       // 空マガ時
	bool    canInterruptReload = true;

	// --- Projectile / Hitscan ---
	bool    isProjectile = true;
	float   projectileSpeed = 90.0f;
	float   maxRange = 100.0f;
	float   traceRadius = 0.0f;          // hitscan用（今は未使用でも保持）
	float   muzzleForwardOffset = 0.35f; // projectileData.spawnForwardOffset を反映

	// --- Burst / Charge ---
	int32_t burstCount = 0;
	float   burstIntervalSec = 0.0f;
	float   maxChargeTime = 0.0f;

	// --- Handling ---
	float accuracy = 0.8f;
	float spreadIncrease = 0.01f;

	// 散布界（実際に使う）
	float baseHipSpreadDeg = 1.0f;
	float baseAdsSpreadDeg = 0.25f;
	float spreadRecoveryRate = 8.0f; // 度/秒
	float maxSpreadDeg = 6.0f;

	// 互換（既存）
	float recoilRecovery = 8.0f;

	// --- ADS ---
	float adsZoomFov = 60.0f;
	float adsTransitionSpeed = 10.0f;

	// --- Pellet ---
	int32_t pelletCount = 1;
	float   pelletSpreadAngle = 0.0f; // ペレット用の追加拡散（度）
};