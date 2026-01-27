#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

/// ---------- レアリティ定義 ---------- ///
enum class EWeaponRarity : uint8_t
{
	Common,      // コモン
	Rare,        // レア
	Epic,        // エピック
	Legendary,   // レジェンダリー
	Mythical,	 // ミシカル
};

/// ---------- 弾薬タイプ定義 ---------- ///
enum class EAmmoType : uint8_t
{
	Default,    // デフォルト
	Energy,     // エネルギー
	Explosive,  // 爆発物
	None, 		// なし（近接武器等）
};

/// ---------- カテゴリー定義 ---------- ///
enum class EWeaponCategory : uint8_t
{
	Primary,   // プライマリ
	Backup,    // バックアップ
	Melee,     // 近接
	Special,   // 特殊
	Sniper,    // スナイパー
	Heavy      // ヘビー
};

/// ---------- 特殊能力定義 ---------- ///
enum class EWeaponAttribute : uint16_t
{
	None,		// なし
	Poison,		// 毒
	Burning,	// 発火
	AreaDamage,	// 爆風ダメージ
	Bouncing,	// 反射弾
	LifeSteal,	// 吸血
	WallBreak,	// 壁貫通
	FixedDelay,	// 固定ディレイ
};

/// ---------- アニメーション定義 ---------- ///
enum class EWeaponAnimation : uint8_t
{
	Equip,          // 装備
	Idle,           // 待機
	Fire,           // 発射
	Reload,         // リロード
	MeleeAttack,    // 近接攻撃
};

/// ---------- サウンド定義 ---------- ///
enum class EWeaponSound : uint8_t
{
	Fire,           // 発射音
	Reload,         // リロード音
	Impact,         // ヒット音
	Empty,          // 空撃ち音
	Equip,          // 装備音
};

/// ---------- 基本構造体 ---------- ///
struct FWeaponCore
{
	int32_t weaponID = 0;									// 武器ID
	std::string weaponName = "Weapon";						// 武器名
	EWeaponCategory category = EWeaponCategory::Primary;	// 武器カテゴリー
	EWeaponRarity rarity = EWeaponRarity::Common;           // レアリティ
};

/// ---------- 射撃性能データ ---------- ///
struct FWeaponStats
{
	float damage = 10.0f;                       // ダメージ
	float fireRate = 600.0f;                    // 発射速度（RPM）
	float mobility = 1.0f;                      // 機動性
	int32_t capacity = 30;                      // 弾薬容量
	EAmmoType ammoType = EAmmoType::Default;	// 弾薬タイプ
	float reloadTime = 1.6f;                    // リロード時間（秒）
	int32_t ammoPerShot = 1;                    // 1発あたりの消費弾薬数
	int32_t maxReserveAmmo = 90;				// 最大予備弾薬数

	// クリティカル関連
	float criticalChance = 0.05f;               // クリティカルヒット確率
	float headshotMultiplier = 1.5f;            // ヘッドショットダメージ倍率
};

/// ---------- 操作・反動データ ---------- ///
struct FWeaponHandling
{
	float accuracy = 0.8f;                      // 精度
	float spreadIncrease = 0.01f;				// 射撃ごとの拡散増加量
	float verticalRecoil = 0.02f;               // 垂直反動
	float horizontalRecoil = 0.1f;              // 水平反動
	float recoilRecovery = 8.0f;				// 反動回復速度

	// エイムダウンサイト（ADS）関連
	float adsZoomFov = 60.0f;					// ADS時の視野角
	float adsTransitionSpeed = 10.0f;			// ADSへの移行速度
	float fixedDelayTime = 10.0f;				// 武器切替時の固定ディレイ時間（秒）
	float zoomLevel = 1.0f;						// スコープ倍率
};

/// ---------- 弾道・判定詳細データ ---------- ///
struct FWeaponProjectileData
{
	// 射程・弾道
	float maxRange = 0.0f;						// 最大射程
	bool bIsProjectile = false;                 // 弾道武器かどうか
	float projectileSpeed = 0.0f;               // 弾道速度（弾道武器の場合）

	int32_t pierceCount = 0;					// 貫通可能な敵数
	int32_t ricochetCount = 0;					// 反射可能な回数
	float splashRadius = 0.0f;					// 爆風半径（爆発物の場合）
	bool bCanDamageSelf = false;				// 自己ダメージ可能かどうか
};

/// ---------- バースト設定データ ---------- ///
struct FBurstSettings
{
	int32_t count = 0;						// バースト内の射撃数
	float interval = 0.0f;                 // バースト内の射撃間隔（秒）
};

/// ---------- チャージ設定データ ---------- ///
struct FChargeSettings
{
	float maxChargeTime = 0.0f;	// 最大チャージ時間（秒）
};

/// ---------- 近接武器専用データ ---------- ///
struct FMeleeWeaponData
{
	// 攻撃範囲・判定
	float attackRange = 2.0f;			 // 攻撃範囲
	float attackArc = 90.0f;			 // 攻撃角度
	float verticalAngle = 20.0f;		 // 垂直攻撃角度

	// タイミング
	float startupDelay = 0.1f;			 // 攻撃ボタンを押してから判定が出るまでの時間（予備動作）
	float activeFrames = 0.15f;			 // 攻撃判定が持続する時間
	float recoveryDelay = 0.25f;		 // 振り終わった後の硬直時間

	// コンボ設定
	int32_t maxComboCount = 3;			 // 最大コンボ数
	float comboWindow = 0.25f;			 // 次のコンボ入力を受け付ける猶予時間

	// 特殊アクション
	float dashImpulse = 3.0f;			 // 攻撃時の前進ダッシュ速度
	bool bCanCharge = false;			 // チャージ攻撃が可能かどうか
	float maxChargeTime = 0.0f;			 // 最大チャージ時間
	float chargeDamageMultiplier = 1.0f; // チャージ時のダメージ倍率

	// 防御挙動
	bool bCanBlock = false;				 // ブロック可能かどうか
	float blockDamageReduction = 0.5f;	 // ブロック時のダメージ軽減率

	// ヒット時の挙動
	float hitStopDuration = 0.2f;		 // 当たった瞬間にゲームを止める時間（手応えの演出）
	float knockbackForce = 30.0f;		 // 敵を弾き飛ばす力
};

/// ---------- 決済データ ---------- ///
struct FWeaponEconomyData
{
	std::string currencyType = "";	// 通貨タイプ（例："Gold", "Gems"）
	int32_t purchasePrice = 100;	// 購入価格
	int32_t minLevelToUnlock = 0;	// 解放に必要なプレイヤーレベル

	// アップグレードコスト（レベルごと）
	std::vector<int32_t> upgradeCosts = { 100, 200, 300 };

	// セール情報
	bool bIsLimitedTime = false;	// 限定販売フラグ
	float discountRate = 0.0f;		// 割引率（例：0.2は20%オフ）
};

/// ---------- アセットデータ ---------- ///
struct FWeaponAssets
{
	std::string modelPath = "";						// 3Dモデルのパス
	std::string iconPath = "";						// アイコン画像のパス
};

/// ---------- サウンドデータ ---------- ///
struct FWeaponSounds
{
	std::string fireSoundPath = "";					// 発射音のパス
	std::string reloadSoundPath = "";				// リロード音のパス
	std::string emptySoundPath = "";				// 空撃ち音のパス
	std::string equipSoundPath = "";				// 装備音のパス
	std::string impactSoundPath = "";				// ヒット音のパス
};

/// ---------- メイン構造体 ---------- ///
struct FWeaponMasterData
{
	/// ---------- 共通データ ---------- ///
	FWeaponCore coreData;

	/// ---------- 経済データ ---------- ///
	FWeaponEconomyData economyData;

	/// ---------- 射撃武器の場合のみ使用するデータ ---------- ///
	FWeaponStats stats;

	/// ---------- 操作・反動データ ---------- ///
	FWeaponHandling handling;

	/// ---------- アセットデータ ---------- ///
	FWeaponAssets assetData;

	/// ---------- サウンドデータ ---------- ///
	FWeaponSounds soundData;

	/// ---------- 近接武器の場合のみ使用するデータ ---------- ///
	std::optional<FMeleeWeaponData> meleeData;			 // 近接武器専用データ

	// 弾道・判定詳細データ（弾道武器の場合のみ）
	std::optional<FWeaponProjectileData> projectileData;

	// バースト設定（バースト武器の場合のみ）
	std::optional<FBurstSettings> burstSettings;

	// チャージ設定（チャージ武器の場合のみ）
	std::optional<FChargeSettings> chargeSettings;

	// 特殊能力
	bool bIsAutomatic = false;				  // フルオートかどうか
	std::vector<EWeaponAttribute> attributes; // 武器の特殊能力リスト
};