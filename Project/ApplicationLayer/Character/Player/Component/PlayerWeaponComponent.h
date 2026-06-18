#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

#include "WeaponSystem.h"
#include "WeaponMasterData.h"

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Camera; }
class BulletManager;
class CollisionManager;
struct InputSnapshot;

/// --------------------------------------------------------------
///				プレイヤーの武器管理コンポーネントクラス
/// ---------------------------------------------------------------
class PlayerWeaponComponent
{
private: /// ---------- 構造体 ---------- ///

	struct AmmoView
	{
		bool usesAmmo = false;
		int mag = 0;
		int reserve = 0;
	};

	struct SavedWeaponState
	{
		bool valid = false;
		int magAmmo = 0;
		int reserveAmmo = 0;
		bool fireModeAutomatic = false;
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	PlayerWeaponComponent() = default;

	// WeaponMasterData の読み込みディレクトリを外部から指定
	// 例: "Resources/JSON/weapons" (primary/backup/... のカテゴリフォルダがある root)
	void SetMasterDirectory(const std::filesystem::path& dir);
	const std::filesystem::path& GetMasterDirectory() const { return weaponMasterDir_; }

	// 明示ロード（Initialize で呼びたい時用）
	bool LoadWeaponMasterDataOnce();

	// 1フレーム分：入力から武器の挙動を処理（切替/モード/近接リマップ/内部Tick）
	// ※ snapshot を「近接カテゴリ時に fire→melee へ変換」するため参照渡し。
	void UpdateAndHandleInput(float dt, InputSnapshot& snapshot);

	// 状態
	bool IsLoaded() const { return weaponLoaded_; }
	const std::string& GetLoadError() const { return weaponLoadError_; }
	bool IsMeleeCategory() const { return weaponCategory_ == EWeaponCategory::Melee; }
	bool IsHeavyCategory() const { return weaponCategory_ == EWeaponCategory::Heavy; }
	bool CanCurrentWeaponRecoverAmmo() const;
	std::string GetCurrentWeaponName() const;

	// HUD用
	bool GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec) const;
	bool ShouldShowNoAmmoUI() const;

	// 外部から装備
	bool EquipWeaponByID(int32_t weaponID);
	void EquipWeaponById(int32_t weaponID) { (void)EquipWeaponByID(weaponID); }

	// Combat FSM から呼ぶ最小API
	bool CanFire(const InputSnapshot& snapshot) const;
	bool TryFire(const InputSnapshot& snapshot, Ken4lowEngine::Camera* shootCamera, BulletManager* bulletManager, CollisionManager* collisionManager);

	// リロード完了判定と開始コマンド
	bool IsReloadFinished() const;
	void StartReload();

	// 自動リロード
	bool ShouldAutoReload() const;
	bool TryAutoReload();

	// リロードをキャンセルできる場合はキャンセルする
	void CancelReload();

	// 強制終了（キャンセルできないリロードもこれで止める）
	void AbortReload();

	// リロード終了（キャンセル/中断も含む）
	void StopReload();

	// 明示的に許可された時だけキャンセル
	void CancelReloadForced();

	// ImGuiの描画処理（デバッグ用）
	void DrawImGui();

	bool ReloadWeaponMasterDataAndReequip();
	bool RebuildCurrentWeaponFromDatabase();

	WeaponMasterDataDatabase& GetWeaponMasterDatabase() { return weaponSys_.Database(); }
	const WeaponMasterDataDatabase& GetWeaponMasterDatabase() const { return weaponSys_.Database(); }
	int32_t GetCurrentWeaponId() const { return currentWeaponId_; }

	bool GetReticleUI(FWeaponReticleData& outReticle, float& outSpread, bool& outIsADS) const;

	int GetSelectedHot_barIndex() const;
	AmmoView GetAmmoViewByHot_barIndex(int hotbarIndex) const;

	bool GetCurrentAdsViewTuning(float& outAdsFovDeg, float& outAdsTransitionSpeed) const;
	bool GetCurrentAdsMoveMultiplier(float& outAdsMoveMul) const;

	void SwitchWeaponCategoryByDelta(int delta);
	void SetAllowedHotbarSlotCount(int count);
	int GetAllowedHotbarSlotCount() const { return allowedHotbarSlotCount_; }

	int AddReserveAmmo(int amount);
	int GetMagazineAmmo() const;
	int GetMagazineCapacity() const;
	int GetReserveAmmo() const;
	int GetMaxReserveAmmo() const;
	bool AddCurrentWeaponAmmo(int amount);

private: /// ---------- メンバ関数 ---------- ///

	void TickWeapon(float dt);
	void SwitchWeaponByDelta(int delta);
	void SwitchWeaponCategory(EWeaponCategory category);
	bool IsHotbarSlotAllowed(int slot) const;
	void ApplyMeleeInputRemap(InputSnapshot& snapshot);

	void BuildInitialAmmoViewCacheFromMasterData();
	void UpdateSelectedAmmoViewCache() const;

	void SaveCurrentWeaponState();
	void RestoreWeaponState(int32_t weaponID);

private: /// ---------- メンバ変数 ---------- ///

	// ---- Weapon system ----
	std::filesystem::path weaponMasterDir_ = "Resources/JSON/weapons"; // primary/backup/... がある root
	WeaponSystem weaponSys_{};
	bool weaponLoaded_ = false;
	std::string weaponLoadError_;
	EWeaponCategory weaponCategory_ = EWeaponCategory::Primary; // 現在扱うカテゴリ
	std::vector<int32_t> weaponIdList_;
	int32_t currentWeaponId_ = 0;
	std::array<int32_t, 6> lastWeaponIdByCategory_{}; // category index -> last equipped id
	bool lastAimHeld_ = false;
	int allowedHotbarSlotCount_ = 6;

	// HUD表示用：スロットごとの弾薬表示キャッシュ
	mutable std::array<AmmoView, 6> ammoViewCache_{};
	mutable std::array<bool, 6> ammoViewCacheValid_{};

	std::unordered_map<int32_t, SavedWeaponState> savedWeaponStates_;
};

