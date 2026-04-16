#pragma once
#include "ReloadCircle.h"
#include "Crosshair.h"
#include "HPWidget.h"
#include "WeaponSlot.h"
#include "WaveUI.h"
#include "DamageIndicatorManager.h"
#include "NoAmmoUI.h"
#include "ControlGuideUI.h"

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Player;

/// -------------------------------------------------------------
///                     HUDマネージャークラス
/// -------------------------------------------------------------
class HUDManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update(float deltaTime);

	// 描画処理
	void Draw();

public: /// ---------- セッタ ---------- ///

	void SetPlayer(Player* player) { player_ = player; }

	// HUDへ渡す値（GamePlaySceneなどから呼ぶ）
	void SetHP(float hp, float maxHp);
	void NotifyPlayerHit(float strength01 = 1.0f);

	// クロスヘア（敵ヒット/撃破通知）
	void NotifyEnemyHit(bool isHeadshot = false);
	void NotifyEnemyKill(bool isHeadshot = false);

	// クロスヘア（移動状態は外部から渡す：APEX風拡散）
	void SetCrosshairMovementState(bool isMoving, bool isSprinting, bool isAirborne);
	void NotifyCrosshairLanded();

	// 必要なら外から切り替え
	void SetCrosshairVisible(bool v) { if (crosshair_) crosshair_->SetVisible(v); }
	void SetReloadCircleVisible(bool v) { if (reloadCircle_) reloadCircle_->SetVisible(v); }
	void SetHPVisible(bool v) { if (hpWidget_) hpWidget_->SetVisible(v); }

	// WeaponSlot のHUDスナップショットを受け取る（HUD側でWeaponSlotの状態を参照して描画するため）
	void SetWeaponSlotSnapshot(const WeaponSlot::HudSnapshot& snapshot) { weaponSlotSnapshot_ = snapshot; }

	void SetWaveDisplayState(const WaveUI::DisplayState& state);
	void NotifyWaveStarted(int waveNumber, bool isFinalWave);
	void NotifyAllWavesCleared();
	void SetWaveUIVisible(bool v);

	void AddDamageIndicator(const K4E::Vector3& playerPos,
		const K4E::Vector3& attackerPos,
		const K4E::Vector3& cameraForward,
		const K4E::Vector3& cameraRight);

	void SetCrosshairTargetingEnemy(bool v);
	void SetControlGuideVisible(bool v) { if (controlGuideUI_) controlGuideUI_->SetVisible(v); }

public: /// ---------- ゲッタ ---------- ///

	ReloadCircle* GetReloadCircle() const { return reloadCircle_.get(); }
	Crosshair* GetCrosshair() const { return crosshair_.get(); }
	HPWidget* GetHPWidget() const { return hpWidget_.get(); }
	WeaponSlot* GetWeaponSlot() const { return weaponSlot_.get(); }
	ControlGuideUI* GetControlGuideUI() const { return controlGuideUI_.get(); }

private: /// ---------- メンバ変数 ---------- ///

	Player* player_ = nullptr; // プレイヤーへの参照（HUDがゲーム状態を参照するため）

	std::unique_ptr<ReloadCircle> reloadCircle_; // リロード円
	std::unique_ptr<Crosshair> crosshair_; // 十字照準
	std::unique_ptr<HPWidget> hpWidget_; // HPウィジェット

	std::unique_ptr<WaveUI> waveUI_; // ウェーブUI（WaveDefense用）
	std::unique_ptr<ControlGuideUI> controlGuideUI_; // コントロールガイドUI

	std::unique_ptr<WeaponSlot> weaponSlot_; // 武器スロット
	WeaponSlot::HudSnapshot weaponSlotSnapshot_{};    // 外部から受け取る表示用データ

	std::unique_ptr<DamageIndicatorManager> damageIndicatorManager_;

	std::unique_ptr<NoAmmoUI> noAmmoUI_;

	bool reloadTimerIsRemaining_ = true; // リロードタイマーが「残り時間」か「経過時間」かのフラグ
	bool prevReloading_ = false; // 前フレームのリロード状態（HUDの更新に使う）
};
