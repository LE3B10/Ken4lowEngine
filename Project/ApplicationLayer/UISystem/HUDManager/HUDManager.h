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
///
/// GamePlayWorldが所有し、プレイヤーHP、照準、リロード、武器スロット、
/// Wave表示、被弾方向、弾切れ、操作ガイドなどGamePlay中のHUDをまとめて管理する。
/// Playerは参照のみ保持し、寿命はCharacterWorld/GamePlayWorld側に従う。
/// -------------------------------------------------------------
class HUDManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 各HUD部品を生成し、テクスチャパス・初期位置・表示状態を設定する。
	void Initialize();

	// Playerから現在HP/武器/リロード/弾切れ状態を取得し、HUD部品を1フレーム更新する。
	void Update(float deltaTime);

	// 可視状態のHUD部品を、画面上の重なり順に描画する。
	void Draw();

public: /// ---------- セッタ ---------- ///

	void SetPlayer(Player* player) { player_ = player; }

	// GamePlayWorldから渡された現在HP/最大HPをHPWidgetへ反映する。
	void SetHP(float hp, float maxHp);
	// 被弾時のHP表示リアクションを発火する。
	void NotifyPlayerHit(float strength01 = 1.0f);

	// 敵命中/撃破時のクロスヘアマーカーを発火する。
	void NotifyEnemyHit(bool isHeadshot = false);
	void NotifyEnemyKill(bool isHeadshot = false);

	// Player移動状態をクロスヘア拡散へ反映する。移動判定そのものはPlayer側が持つ。
	void SetCrosshairMovementState(bool isMoving, bool isSprinting, bool isAirborne);
	void NotifyCrosshairLanded();

	// 必要なら外から切り替え
	void SetCrosshairVisible(bool v) { if (crosshair_) crosshair_->SetVisible(v); }
	void SetReloadCircleVisible(bool v) { if (reloadCircle_) reloadCircle_->SetVisible(v); }
	void SetHPVisible(bool v) { if (hpWidget_) hpWidget_->SetVisible(v); }

	// WeaponSlot のHUDスナップショットを受け取る（HUD側でWeaponSlotの状態を参照して描画するため）
	void SetWeaponSlotSnapshot(const WeaponSlot::HudSnapshot& snapshot) { weaponSlotSnapshot_ = snapshot; }

	// WaveManagerの進行状態をWaveUIへ反映する。
	void SetWaveDisplayState(const WaveUI::DisplayState& state);
	// Wave開始演出を通知する。最終Waveかどうかで表示文言を切り替える。
	void NotifyWaveStarted(int waveNumber, bool isFinalWave);
	// 全Wave完了演出を通知する。
	void NotifyAllWavesCleared();
	void SetWaveUIVisible(bool v);

	// 攻撃方向を画面上の被弾インジケータへ変換して追加する。
	void AddDamageIndicator(const K4E::Vector3& playerPos,
		const K4E::Vector3& attackerPos,
		const K4E::Vector3& cameraForward,
		const K4E::Vector3& cameraRight);

	// 照準線上に敵がいるかをクロスヘア色/状態へ反映する。
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
