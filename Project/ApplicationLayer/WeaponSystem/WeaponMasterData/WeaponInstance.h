#pragma once

#include <cstdint>
#include "WeaponParams.h"

class BulletManager;
class CollisionManager;

namespace Ken4lowEngine { class Camera; }
namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///  WeaponRuntimeState
///  - 実行時に変化する値（残弾/クールダウン/リロード/拡散など）
/// -------------------------------------------------------------
struct WeaponRuntimeState
{
	int32_t magAmmo = 0;
	int32_t reserveAmmo = 0;

	float fireCooldown = 0.0f;

	bool isReloading = false;
	bool reloadRequested = false;
	bool reloadRequest = false;
	bool pendingReload = false;
	float reloadTimer = 0.0f;

	int32_t burstRemaining = 0;
	float   burstTimer = 0.0f;

	// 動的拡散（度）: baseHip/baseAds に加算される
	float spread = 0.0f;

	// ✅ ADS状態（Player側から毎フレーム流す）
	bool isADS = false;
};

/// -------------------------------------------------------------
///  WeaponInstance
///  - Playerは入力を渡すだけ
///  - 弾生成/クールダウン/リロード/バースト等はここで完結
/// -------------------------------------------------------------
class WeaponInstance
{
public:
	void Equip(const WeaponParams& p);

	void Tick(float dt);

	void StartReload();

	/// fireHeld: 押しっぱ / firePressed: 押した瞬間
	void TryFire(bool fireHeld, bool firePressed,
		K4E::Camera* cam,
		BulletManager* bulletMgr,
		CollisionManager* colMgr);

	// 発射モード（現在）
	bool IsAutomatic() const { return fireModeAutomatic_; }
	bool CanToggleFireMode() const { return params_.canToggleFireMode; }
	bool ToggleFireMode();

	// ADS状態を外から反映
	void SetADS(bool v) { st_.isADS = v; }

	// UI用に「今回のリロード時間」を返す
	float GetCurrentReloadDurationSec() const;

	const WeaponParams& Params() const { return params_; }
	const WeaponRuntimeState& State() const { return st_; }
	WeaponRuntimeState& StateMutable() { return st_; }

	// リロードをキャンセルできる場合はキャンセルする
	void CancelReload();

private: /// ---------- メンバ関数 ---------- ///

	bool CanStartReload() const;
	void StartReloadInternal();

	bool WantFire(bool fireHeld, bool firePressed) const;
	bool CanFire() const;

	void ConsumeAmmo();
	void FinishReload();

	void FireShot(K4E::Camera* cam, BulletManager* bulletMgr, CollisionManager* colMgr);

	// 現在の基礎拡散（Hip/ADS）
	float GetBaseSpreadDeg() const;

private:
	WeaponParams params_{};
	WeaponRuntimeState st_{};

	bool fireModeAutomatic_ = false; // 現在の発射モード（true: auto / false: semi）。切替可能な武器は params_.isAutomatic と同初期値
};
