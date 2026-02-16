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
	bool    isAutomatic = false;    // true: 押しっぱで連射 / false: クリック毎
	bool    canToggleFireMode = true; // true: V等でフル/セミを切替可能

	// --- Damage / Fire ---
	float   damage = 10.0f;         // 1発ダメージ
	float   secPerShot = 0.1f;      // 発射間隔（秒/発）  ※ RPM -> sec の変換はここに入れる

	int32_t magCapacity = 30;       // マガジン容量
	int32_t ammoPerShot = 1;        // 1発あたりの消費弾薬数
	int32_t maxReserveAmmo = 90;    // 予備弾薬 最大
	float   reloadSec = 1.6f;       // リロード時間

	// --- Projectile / Hitscan ---
	bool    isProjectile = true;    // true: 弾丸をSpawn / false: ヒットスキャン
	float   projectileSpeed = 90.0f;// 弾速（Projectileの場合）
	float   maxRange = 100.0f;      // 射程（Hitscanにも使用）

	// --- Burst / Charge（必要になったら拡張） ---
	int32_t burstCount = 0;         // 0: 無し / >=2: バースト内の弾数
	float   burstIntervalSec = 0.0f;// バースト内の弾間隔

	float   maxChargeTime = 0.0f;   // 0: 無し / >0: チャージ武器

	// --- Handling ---
	float accuracy = 0.8f;          // 0..1（大きいほど正確）
	float spreadIncrease = 0.01f;   // 射撃ごとの拡散増加
	float recoilRecovery = 8.0f;    // 反動/拡散の回復速度（簡易用途）

	// --- ADS ---
	float adsZoomFov = 60.0f;       // ADS時のFOV
	float adsTransitionSpeed = 10.0f;

	// --- Muzzle ---
	float muzzleForwardOffset = 0.35f; // 生成位置をカメラ前方へずらす量（壁/頭めり込み回避）
};
