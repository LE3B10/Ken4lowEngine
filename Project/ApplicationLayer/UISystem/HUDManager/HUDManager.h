#pragma once
#include "ReloadCircle.h"
#include "Crosshair.h"
#include "HPWidget.h"
#include "WeaponSlot.h"
#include "WaveUI.h"
#include "DamageIndicatorManager.h"
#include "NoAmmoUI.h"
#include "ControlGuideUI.h"
#include "Stage1ObjectiveGuideUI.h"
#include "BossHudUI.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <memory>
#include <string>

/// ---------- 前方宣言 ---------- ///
class Player;
class IPlayerRuntime;

/// -------------------------------------------------------------
///                     HUDマネージャークラス
///
/// World/Tutorial/Boss用HUDと、新Player Runtimeから受け取る武器HUDを管理する。
/// 旧ハート・旧Crosshair・旧操作ガイドは比較時だけ明示的に再表示する。
/// -------------------------------------------------------------
class HUDManager
{
public: /// ---------- メンバ関数 ---------- ///

	~HUDManager();

	// 各HUD部品を生成し、テクスチャパス・初期位置・表示状態を設定する。
	void Initialize();

	// World HUD、新Player武器HUD、必要な場合だけ旧Player専用HUDを1フレーム更新する。
	void Update(float deltaTime);

	// World HUD、新Player武器HUD、必要な場合だけ旧Player専用HUDを描画する。
	void Draw();

public: /// ---------- セッタ ---------- ///

	void SetPlayer(Player* player)
	{
		player_ = player;
		SetLegacyPlayerHudVisible(player != nullptr); // 旧Playerを明示接続した場合だけLegacy HUDを復帰する。
	}

	// 新Player Runtimeを参照し、ReloadCircleとWeaponSlotの状態を直接同期する。
	void SetPlayerRuntime(IPlayerRuntime* playerRuntime) { playerRuntime_ = playerRuntime; }
	void SetRuntimeWeaponHudVisible(bool visible);
	bool IsRuntimeWeaponHudVisible() const { return runtimeWeaponHudVisible_; }

	// P13以降はfalseにして、PlayerHudPresenterComponentとの二重描画を防ぐ。
	void SetLegacyPlayerHudVisible(bool visible);
	bool IsLegacyPlayerHudVisible() const { return legacyPlayerHudVisible_; }

	// GamePlayWorldから渡された現在HP/最大HPをHPWidgetへ反映する。
	void SetHP(float hp, float maxHp);
	// ボス本体のHPを参照し、ボス戦用の大型HPバーへ反映する。
	void SetBossHP(float hp, float maxHp, bool bossBattleActive);
	void SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated, bool tutorialActive);
	void SetStage1ObjectiveTutorialAlpha(float alpha);
	void SetStage1ObjectiveTutorialPage(int page);
	void SetStage1ObjectiveTutorialProgress(float progress);
	void SetStage1TutorialItemMarker(int markerIndex, bool visible, const K4E::Vector2& screenPosition, int itemType);
	void NotifyStage1ObjectiveGuideStarted();
	void NotifyStage1BossAppeared();
	// ボス登場演出後、プレイヤーがボスの方向を見失わないようHUDマーカーへ位置情報を渡す。
	void SetBossGuide(const K4E::Vector3& playerPos,
		const K4E::Vector3& bossPos,
		const K4E::Vector3& cameraForward,
		bool bossBattleActive);
	void NotifyBossIntroCompleted(const K4E::Vector3& bossPos);
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
	void SetWeaponSlotVisibleSlotCount(int count);

	// WeaponSlot のHUDスナップショットを受け取る（旧Player比較表示用）。
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
	void SetCrosshairTargetColors(const K4E::Vector4& normalColor, const K4E::Vector4& targetColor);
	void SetControlGuideVisible(bool v) { if (controlGuideUI_) controlGuideUI_->SetVisible(v); }

public: /// ---------- ゲッタ ---------- ///

	ReloadCircle* GetReloadCircle() const { return reloadCircle_.get(); }
	Crosshair* GetCrosshair() const { return crosshair_.get(); }
	HPWidget* GetHPWidget() const { return hpWidget_.get(); }
	WeaponSlot* GetWeaponSlot() const { return weaponSlot_.get(); }
	ControlGuideUI* GetControlGuideUI() const { return controlGuideUI_.get(); }
	bool IsBossHPBarDrawEnabled() const { return bossHudUI_.IsHpBarDrawEnabled(); }
	bool IsWaveUIDrawEnabled() const;

private: /// ---------- メンバ関数 ---------- ///
	// HUD更新を部品単位に分け、Update内で複数責務が混ざらないようにする。
	bool UpdateReloadCircleFromRuntime();
	void UpdateWeaponSlotFromRuntime();
	bool UpdateReloadCircleFromPlayer();
	void UpdateCrosshairFromPlayer(bool isReloadingForHUD);
	void UpdateWeaponSlotFromPlayer();
	void UpdateNoAmmoFromPlayer(float deltaTime);

	IPlayerRuntime* ResolvePlayerRuntime() const;

	Player* player_ = nullptr; // 旧Player HUDを使用する場合だけ設定する非所有参照。
	IPlayerRuntime* playerRuntime_ = nullptr; // 新PlayerのHP・武器状態を参照する非所有Runtime。
	bool legacyPlayerHudVisible_ = false; // 新Player HUDを正本にするため既定では非表示。
	bool runtimeWeaponHudVisible_ = true; // ReloadCircleとWeaponSlotは新Player Runtimeから表示する。

	std::unique_ptr<ReloadCircle> reloadCircle_; // リロード円
	std::unique_ptr<Crosshair> crosshair_; // 十字照準
	std::unique_ptr<HPWidget> hpWidget_; // HPウィジェット

	std::unique_ptr<WaveUI> waveUI_; // ウェーブUI（WaveDefense用）
	std::unique_ptr<ControlGuideUI> controlGuideUI_; // コントロールガイドUI

	std::unique_ptr<WeaponSlot> weaponSlot_; // 武器スロット
	WeaponSlot::HudSnapshot weaponSlotSnapshot_{}; // 旧Player比較表示用スナップショット

	std::unique_ptr<DamageIndicatorManager> damageIndicatorManager_;

	std::unique_ptr<NoAmmoUI> noAmmoUI_;

	bool reloadTimerIsRemaining_ = true; // 旧Playerのリロードタイマー表現を判定するフラグ
	bool prevReloading_ = false; // 前フレームのリロード状態（HUDの更新に使う）
	BossHudUI bossHudUI_; // ボスHPバーとボス方向ガイドを担当するHUD部品
	Stage1ObjectiveGuideUI stage1ObjectiveGuideUI_; // ステージ1専用の目的表示とチュートリアル文言を担当するHUD部品
};