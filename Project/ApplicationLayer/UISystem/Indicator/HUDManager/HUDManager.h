#pragma once
#include "ReloadCircle.h"
#include "Crosshair.h"
#include "HPWidget.h"
#include "WeaponSlot.h"
#include "WaveUI.h"
#include "DamageIndicatorManager.h"
#include "NoAmmoUI.h"
#include "ControlGuideUI.h"
#include "TextSpriteDrawer.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <memory>
#include <string>

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

	~HUDManager();

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
	// ボス本体のHPを参照し、ボス戦用の大型HPバーへ反映する。
	void SetBossHP(float hp, float maxHp, bool bossBattleActive);
	void SetStage1ObjectiveGuide(bool enabled, int destroyedCrystals, int totalCrystals, bool bossBattleActive, bool bossDefeated);
	void NotifyStage1ObjectiveGuideStarted();
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
	void SetCrosshairTargetColors(const K4E::Vector4& normalColor, const K4E::Vector4& targetColor);
	void SetControlGuideVisible(bool v) { if (controlGuideUI_) controlGuideUI_->SetVisible(v); }

public: /// ---------- ゲッタ ---------- ///

	ReloadCircle* GetReloadCircle() const { return reloadCircle_.get(); }
	Crosshair* GetCrosshair() const { return crosshair_.get(); }
	HPWidget* GetHPWidget() const { return hpWidget_.get(); }
	WeaponSlot* GetWeaponSlot() const { return weaponSlot_.get(); }
	ControlGuideUI* GetControlGuideUI() const { return controlGuideUI_.get(); }
	bool IsBossHPBarDrawEnabled() const { return bossHpBarRuntimeVisible_; }
	bool IsWaveUIDrawEnabled() const;

private: /// ---------- メンバ変数 ---------- ///
	struct BossHpBarSettings
	{
		bool visible = true;
		K4E::Vector3 position{ 960.0f, 54.0f, 0.0f };
		float width = 760.0f;
		float height = 22.0f;
		K4E::Vector3 nameOffset{ 0.0f, -28.0f, 0.0f };
		std::string displayName = "GUARDIAN";
		bool showAfterIntro = true;
		bool hideWaveUI = true;
	};

	struct BossGuideSettings
	{
		bool visible = true;
		K4E::Vector2 center{ 960.0f, 540.0f };
		float radius = 155.0f;
		float holdTime = 8.0f;
		float lineThickness = 6.0f;
		float dotSize = 24.0f;
	};

	struct Stage1ObjectiveGuideSettings
	{
		bool visible = true;
		K4E::Vector2 center{ 960.0f, 132.0f };
		K4E::Vector2 panelSize{ 620.0f, 94.0f };
		float titleScale = 0.92f;
		float progressScale = 0.58f;
		float introHoldTime = 7.0f;
	};

	void RegisterBossHpBarParameters();
	void ApplyBossHpBarParameters();
	void InitializeBossHpBarSprites();
	void UpdateBossHpBarSprites();
	void DrawBossHpBar();
	void InitializeBossGuideSprites();
	void UpdateBossGuideSprites(float deltaTime);
	void DrawBossGuide();
	void InitializeStage1ObjectiveGuide();
	void UpdateStage1ObjectiveGuideSprites(float deltaTime);
	void DrawStage1ObjectiveGuide();

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
	BossHpBarSettings bossHpBarSettings_{};
	std::unique_ptr<K4E::Sprite> bossHpFrameSprite_;
	std::unique_ptr<K4E::Sprite> bossHpBackSprite_;
	std::unique_ptr<K4E::Sprite> bossHpDelaySprite_;
	std::unique_ptr<K4E::Sprite> bossHpFillSprite_;
	bool bossBattleActive_ = false;
	bool bossHpBarRuntimeVisible_ = false;
	float bossHp_ = 0.0f;
	float bossMaxHp_ = 0.0f;
	float bossHpRate_ = 0.0f;
	float bossDelayedHpRate_ = 0.0f;
	BossGuideSettings bossGuideSettings_{};
	std::unique_ptr<K4E::Sprite> bossGuideLineSprite_;
	std::unique_ptr<K4E::Sprite> bossGuideDotBackSprite_;
	std::unique_ptr<K4E::Sprite> bossGuideDotSprite_;
	bool bossGuideActive_ = false;
	float bossGuideTimer_ = 0.0f;
	float bossGuideAngle_ = 0.0f;
	float bossGuideLineLength_ = 0.0f;
	K4E::Vector2 bossGuideLineCenter_{};
	K4E::Vector2 bossGuideDotPosition_{};
	K4E::Vector3 bossGuideBossPosition_{};
	Stage1ObjectiveGuideSettings stage1ObjectiveGuideSettings_{};
	std::unique_ptr<K4E::Sprite> stage1ObjectiveGuideBackSprite_;
	std::unique_ptr<K4E::Sprite> stage1ObjectiveGuideAccentSprite_;
	std::unique_ptr<K4E::TextSpriteDrawer> stage1ObjectiveTextDrawer_;
	bool stage1ObjectiveGuideEnabled_ = false;
	bool stage1ObjectiveTextReady_ = false;
	int stage1DestroyedCrystals_ = 0;
	int stage1TotalCrystals_ = 0;
	bool stage1BossBattleActive_ = false;
	bool stage1BossDefeated_ = false;
	float stage1ObjectiveIntroTimer_ = 0.0f;
	float stage1ObjectiveGuideAlpha_ = 0.0f;
};
