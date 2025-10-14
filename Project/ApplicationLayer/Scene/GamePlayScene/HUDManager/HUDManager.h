#pragma once
#include <NumberSpriteDrawer.h>
#include <ReloadCircle.h>
#include "Weapon.h" // Weaponを使うために追加

#include <algorithm>

/// ---------- 武器のカテゴリ ---------- ///
enum class WeaponCategory
{
	Primary, // 主要武器
	Backup,  // 補助武器
	Melee,	 // 近接武器
	Special, // 特殊武器
	Sniper,	 // スナイパー
	Heavy,	 // ヘビー
	Unknown,
};

/// -------------------------------------------------------------
/// 				　		HUDマネージャー
/// -------------------------------------------------------------
class HUDManager
{
private: /// ---------- 構造体 ---------- ///

	// クイックスロット構造体
	struct QuickSlot
	{
		std::unique_ptr<Sprite> background; // 背景
		std::unique_ptr<Sprite> icon; // アイコン
		std::unique_ptr<Sprite> lock; // 錠前アイコン
		int ammoInClip = 0;  // 現在の弾薬数
		int ammoReserve = 0; // 予備の弾薬数
	};
	std::array<QuickSlot, 6> quickSlots_; // クイックスロット3つ分
	int activeSlot_ = 0; // 現在選択中のスロットインデックス

	// ウェーブバナー背景
	std::unique_ptr<Sprite> waveBanner_;

	// 目標矢印
	std::unique_ptr<Sprite> objectiveArrow_;
	float arrowTimer_ = 0.0f;

	// 右側ガジェット(3スロ)
	std::array<std::unique_ptr<Sprite>, 3> gadgetBg_{};
	std::array<std::unique_ptr<Sprite>, 3> gadgetIcon_{};

	// 残り体数アイコン
	struct ActionHint
	{
		std::unique_ptr<Sprite> icon; // LMB / RMB / Rのアイコン
		std::unique_ptr<Sprite> label; // "Shoot" / "Aim" / "Reload" のラベル
		Vector2 position; // 画面上の位置
	};

	std::unique_ptr<Sprite> actionPanel_;
	ActionHint hintFire_, hintAds_, hintReload_;
	float actionPulseT_ = 0.0f;

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// アクションヒントの状態同期
	void SyncActionStates(bool canFire, bool isADS, bool isReloading) { stateCanFire_ = canFire; stateIsADS_ = isADS; stateReloading_ = isReloading; }

public: /// ---------- ゲッター ---------- ///

	ReloadCircle* GetReloadCircle() { return reloadCircle_.get(); }

public: /// ---------- セッター ---------- ///

	// スコアの設定
	void SetScore(int score) { score_ = score; }

	// キル数の設定
	void SetKills(int kills) { kills_ = kills; }

	// 弾薬数の設定
	void SetAmmo(int current, int reserve) { ammoInClip_ = current; ammoReserve_ = reserve; }

	// HPの設定
	void SetHP(float current, float max) { hp_ = current; maxHP_ = max; }

	// リロード中の設定
	void SetReloading(bool isReloading, float progress);

	// 武器を設定
	void SetWeapon(Weapon* weapon) { if (reloadCircle_) reloadCircle_->SetWeapon(weapon); }

	// 弾薬情報を武器から設定する
	void SetAmmoFromWeapon(const Weapon* weapon) { if (weapon) { ammoInClip_ = weapon->GetAmmoInClip(); ammoReserve_ = weapon->GetAmmoReserve(); } }

	void SetWaveInfo(int currentWave, int totalWaves);

	void SetEnemiesRemaining(int remaining);

	void SetActiveWeaponIndex(int idx);
	void SyncWeaponSlots(int numWeapons);

	// 今使っている武器タイプをHUDに伝える
	void SetActiveWeaponCategory(WeaponCategory cat) { activeCategory_ = cat; }

private: // ---------- 初期化ヘルパ ----------

	void InitNumberDrawers(); // 数字描画用
	void InitBars();		  // HPバー、リロード円
	void InitQuickSlots();	  // クイックスロット
	void InitWaveAndArrow();  // ウェーブバナー、目標矢印
	void InitGadgets();		  // ガジェット
	void InitCategoryIcons(); // カテゴリアイコン
	void InitActionHints();   // アクションヒント

private: // ---------- 更新ヘルパ ----------

	void UpdateHPBar();			   // HPバー更新
	void UpdateQuickSlotsLayout(); // クイックスロットのレイアウト更新
	void UpdateCategoryBadge();	   // カテゴリアイコン更新
	void UpdateActionHints();	   // アクションヒント更新
	void UpdateWaveBanner();	   // ウェーブバナー更新
	void UpdateObjectiveArrow();   // 目標矢印更新
	void UpdateGadgets();		   // ガジェット更新

private: // ---------- 描画ヘルパ ----------

	void DrawAmmoNumbers();	   // 弾薬数の描画
	void DrawWaveRemainOnly(); // ウェーブと残り体数のみ描画
	void DrawHPAndReload();	   // HPとリロード円の描画
	void DrawActionHints();	   // アクションヒントの描画
	void DrawQuickSlots();	   // クイックスロットの描画
	void DrawCategoryBadge();  // カテゴリアイコンの描画
	void DrawGadgets();		   // ガジェットの描画
	void DrawDebugHUD();	   // デバッグ用HUDの描画

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<NumberSpriteDrawer> scoreDrawer_; // スコア表示用
	std::unique_ptr<NumberSpriteDrawer> killDrawer_;  // キル数表示用
	std::unique_ptr<NumberSpriteDrawer> ammoDrawer_;  // 弾薬数表示用
	std::unique_ptr<NumberSpriteDrawer> hpDrawer_;	  // HP表示用

	std::unique_ptr<Sprite> hpBarBase_; // グレー背景
	std::unique_ptr<Sprite> hpBarFill_; // 緑バー

	std::unique_ptr<ReloadCircle> reloadCircle_; // リロード円

	std::unique_ptr<Sprite> topPanel_;   // 上中央の半透明パネル
	std::unique_ptr<Sprite> ammoPanel_;  // 右下の半透明パネル

	std::unordered_map<WeaponCategory, std::unique_ptr<Sprite>> categoryIcons_;
	WeaponCategory activeCategory_ = WeaponCategory::Unknown;

	// スコア
	int score_ = 0;

	// キル数
	int kills_ = 0;

	// HP
	float hp_ = 100.0f; // プレイヤーのHP
	float maxHP_ = 100.0f;

	// 弾薬数
	int ammoInClip_ = 0;  // 現在の弾薬数
	int ammoReserve_ = 0; // 予備の弾薬数

	int waveCur_ = 1;
	int waveTotal_ = 1;
	int enemiesRemain_ = 0;

	int numWeapons_ = 2; // まずは2本

	// （数字描画用：必要なら）
	std::unique_ptr<NumberSpriteDrawer> waveCurDrawer_;    // 現在Wave
	std::unique_ptr<NumberSpriteDrawer> waveTotalDrawer_;  // 総Wave
	std::unique_ptr<NumberSpriteDrawer> remainDrawer_;     // 残り体数

	const std::string texturePath_ = "Number.png"; // 数字のテクスチャパス

	std::unique_ptr<Sprite> remainIcon_;
	float remainIconPopT_ = 0.0f; // 減った瞬間にポップ演出
	int prevEnemiesRemain_ = -1;

	bool stateCanFire_ = true;
	bool stateIsADS_ = false;
	bool stateReloading_ = false;
};

