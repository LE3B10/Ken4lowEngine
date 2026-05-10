#define NOMINMAX
#include "PlayerWeaponComponent.h"

#include "PlayerInputSnapshot.h" // InputSnapshot の定義（BuildInputSnapshot と同じ）

#include <algorithm>
#include <filesystem>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	// weaponSlotPressed(1..6) -> EWeaponCategory へ変換
	inline bool SlotToCategory(int slot, EWeaponCategory& out)
	{
		switch (slot)
		{
		case 1: out = EWeaponCategory::Primary; return true;
		case 2: out = EWeaponCategory::Backup;  return true;
		case 3: out = EWeaponCategory::Melee;   return true;
		case 4: out = EWeaponCategory::Special; return true;
		case 5: out = EWeaponCategory::Sniper;  return true;
		case 6: out = EWeaponCategory::Heavy;   return true;
		default: return false;
		}
	}

	inline int CategoryToHotbarIndex(EWeaponCategory c)
	{
		switch (c)
		{
		case EWeaponCategory::Primary: return 0;
		case EWeaponCategory::Backup:  return 1;
		case EWeaponCategory::Melee:   return 2;
		case EWeaponCategory::Special: return 3;
		case EWeaponCategory::Sniper:  return 4;
		case EWeaponCategory::Heavy:   return 5;
		default: return -1;
		}
	}
}

void PlayerWeaponComponent::SetMasterDirectory(const std::filesystem::path& dir)
{
	weaponMasterDir_ = dir;
	weaponLoaded_ = false;
	weaponLoadError_.clear();
	weaponIdList_.clear();
	currentWeaponId_ = 0;
	weaponSys_ = WeaponSystem{};

	// HUD弾薬キャッシュをリセット
	ammoViewCache_ = {};
	ammoViewCacheValid_.fill(false);

	savedWeaponStates_.clear();
}

bool PlayerWeaponComponent::LoadWeaponMasterDataOnce()
{
	if (weaponLoaded_) return true;

	// ディレクトリが違う/作業ディレクトリが環境で変わることがあるので候補を順に試す
	std::vector<std::filesystem::path> candidates;
	candidates.push_back(weaponMasterDir_);
	auto addCandidate = [&](const char* p)
		{
			if (weaponMasterDir_ != p) candidates.push_back(p);
		};
	// 新しい配置（推奨）: Resources/JSON/weapons/[category]/*.json
	addCandidate("Resources/JSON/weapons");
	addCandidate("JSON/weapons");
	// 旧配置（互換）
	addCandidate("Resources/WeaponMasterData");
	addCandidate("WeaponMasterData");

	std::string err;
	bool loaded = false;
	for (const auto& dir : candidates)
	{
		if (dir.empty()) continue;
		if (!std::filesystem::exists(dir)) continue;
		if (weaponSys_.Load(dir, &err))
		{
			weaponMasterDir_ = dir;
			loaded = true;
			break;
		}
	}

	if (!loaded)
	{
		weaponLoaded_ = false;
		weaponLoadError_ = err.empty() ? "WeaponMasterData: 読み込みに失敗しました（ディレクトリを確認してください）" : err;
		currentWeaponId_ = 0;
		weaponIdList_.clear();
		return false;
	}

	weaponLoaded_ = true;

	BuildInitialAmmoViewCacheFromMasterData();

	// まずはカテゴリ（デフォルトPrimary）だけをプレイヤーの選択リストにする
	weaponIdList_ = weaponSys_.GetWeaponIdListSortedByCategory(weaponCategory_);
	if (weaponIdList_.empty())
	{
		// そのカテゴリが空なら、全カテゴリのIDリストにフォールバック
		weaponIdList_ = weaponSys_.GetWeaponIdListSorted();
	}
	if (weaponIdList_.empty())
	{
		weaponLoaded_ = false;
		weaponLoadError_ = "WeaponMasterData: データが0件でした。（Resources/JSON/weapons/primary などにjsonがあるか確認してください）";
		currentWeaponId_ = 0;
		return false;
	}

	// 既にIDがあるならそのIDを、なければ先頭を装備
	std::string equipErr;
	if (currentWeaponId_ > 0)
	{
		if (!weaponSys_.EquipById(currentWeaponId_, &equipErr))
			weaponSys_.EquipFirst(&equipErr);
	}
	else
	{
		weaponSys_.EquipFirst(&equipErr);
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_ = equipErr;
	return true;
}

bool PlayerWeaponComponent::GetReticleUI(FWeaponReticleData& outReticle, float& outSpread, bool& outIsADS) const
{
	if (!weaponLoaded_) return false;

	outReticle = weaponSys_.GetEquippedReticleData();
	outSpread = weaponSys_.Weapon().State().spread; // いまの動的拡散値
	outIsADS = lastAimHeld_;
	return true;
}

int PlayerWeaponComponent::GetSelectedHot_barIndex() const
{
	switch (weaponCategory_)
	{
	case EWeaponCategory::Primary: return 0;
	case EWeaponCategory::Backup:  return 1;
	case EWeaponCategory::Melee:   return 2;
	case EWeaponCategory::Special: return 3;
	case EWeaponCategory::Sniper:  return 4;
	case EWeaponCategory::Heavy:   return 5;
	default:                       return -1;
	}
}

PlayerWeaponComponent::AmmoView PlayerWeaponComponent::GetAmmoViewByHot_barIndex(int hotbarIndex) const
{
	AmmoView out{};

	if (hotbarIndex < 0 || hotbarIndex >= 6) return out;
	if (!weaponLoaded_) return out;

	// 選択中スロットだけはランタイム値で毎フレーム更新
	UpdateSelectedAmmoViewCache();

	// 初期キャッシュ or 更新済みキャッシュを返す
	if (ammoViewCacheValid_[hotbarIndex])
	{
		return ammoViewCache_[hotbarIndex];
	}

	// 未設定スロット（そのカテゴリに武器がない等）は非表示
	out.usesAmmo = false;
	return out;
}


bool PlayerWeaponComponent::CanCurrentWeaponRecoverAmmo() const
{
	if (!weaponLoaded_) return false;
	if (weaponCategory_ == EWeaponCategory::Melee) return false;

	return weaponSys_.Weapon().GetMaxReserveAmmo() > 0;
}

std::string PlayerWeaponComponent::GetCurrentWeaponName() const
{
	if (!weaponLoaded_) return "未ロード";

	const FWeaponMasterData* data = weaponSys_.Database().FindByID(currentWeaponId_);
	if (!data) return "不明";
	return data->coreData.weaponName;
}

bool PlayerWeaponComponent::GetCurrentAdsViewTuning(float& outAdsFovDeg, float& outAdsTransitionSpeed) const
{
	if (!weaponLoaded_) return false;

	const auto& p = weaponSys_.Weapon().Params();

	outAdsFovDeg = p.adsZoomFov;
	outAdsTransitionSpeed = p.adsTransitionSpeed;
	return true;
}

bool PlayerWeaponComponent::GetCurrentAdsMoveMultiplier(float& outAdsMoveMul) const
{
	if (!weaponLoaded_) return false;

	const auto& p = weaponSys_.Weapon().Params();

	// WeaponParams 側の名前が違う場合はここだけ合わせてください
	outAdsMoveMul = std::max(0.0f, p.adsMoveSpeedMultiplier);
	return true;
}

void PlayerWeaponComponent::SwitchWeaponCategoryByDelta(int delta)
{
	if (!LoadWeaponMasterDataOnce())
	{
		return;
	}

	if (delta == 0)
	{
		return;
	}

	// 現在カテゴリを 0..5 の hotbar index に変換
	int currentIndex = GetSelectedHot_barIndex();
	if (currentIndex < 0)
	{
		currentIndex = 0;
	}

	currentIndex += (delta > 0) ? 1 : -1;

	if (currentIndex < 0)
	{
		currentIndex = 5;
	}
	if (currentIndex >= 6)
	{
		currentIndex = 0;
	}

	EWeaponCategory nextCategory = EWeaponCategory::Primary;
	switch (currentIndex)
	{
	case 0: nextCategory = EWeaponCategory::Primary; break;
	case 1: nextCategory = EWeaponCategory::Backup;  break;
	case 2: nextCategory = EWeaponCategory::Melee;   break;
	case 3: nextCategory = EWeaponCategory::Special; break;
	case 4: nextCategory = EWeaponCategory::Sniper;  break;
	case 5: nextCategory = EWeaponCategory::Heavy;   break;
	default: return;
	}

	SwitchWeaponCategory(nextCategory);
}

void PlayerWeaponComponent::TickWeapon(float dt)
{
	if (!weaponLoaded_) return;
	weaponSys_.Tick(dt);
}

bool PlayerWeaponComponent::EquipWeaponByID(int32_t weaponID)
{
	if (!weaponLoaded_) LoadWeaponMasterDataOnce();
	if (!weaponLoaded_) return false;

	if (weaponID <= 0) return false;

	// 同じ武器を再装備するだけなら何もしない
	if (weaponID == currentWeaponId_)
	{
		return true;
	}

	// 切替前の武器状態を保存
	SaveCurrentWeaponState();

	std::string err;
	if (!weaponSys_.EquipById(weaponID, &err))
	{
		weaponLoadError_ = err;
		return false;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();

	// 保存済み状態があれば復元
	RestoreWeaponState(currentWeaponId_);

	weaponLoadError_.clear();

	// 新しい選択武器のHUDを更新
	UpdateSelectedAmmoViewCache();

	return true;
}

void PlayerWeaponComponent::SwitchWeaponByDelta(int delta)
{
	if (!weaponLoaded_) LoadWeaponMasterDataOnce();
	if (!weaponLoaded_) return;
	if (weaponIdList_.empty()) return;

	// 現在IDのindexを探す
	int idx = 0;
	for (int i = 0; i < static_cast<int>(weaponIdList_.size()); ++i)
	{
		if (weaponIdList_[i] == currentWeaponId_) { idx = i; break; }
	}

	idx += (delta > 0) ? 1 : -1;
	if (idx < 0) idx = static_cast<int>(weaponIdList_.size()) - 1;
	if (idx >= static_cast<int>(weaponIdList_.size())) idx = 0;

	EquipWeaponByID(weaponIdList_[idx]);
}

void PlayerWeaponComponent::SwitchWeaponCategory(EWeaponCategory category)
{
	// まずロード（失敗したら何もしない）
	if (!LoadWeaponMasterDataOnce())
		return;

	// 現カテゴリの最後の武器IDを記録
	const int curIdx = static_cast<int>(weaponCategory_);
	if (curIdx >= 0 && curIdx < static_cast<int>(lastWeaponIdByCategory_.size()))
		lastWeaponIdByCategory_[curIdx] = currentWeaponId_;

	// 新カテゴリのID一覧を作る
	const auto ids = weaponSys_.GetWeaponIdListSortedByCategory(category);
	if (ids.empty())
	{
		weaponLoadError_ = "WeaponMasterData: このカテゴリに武器データがありません。";
		return;
	}

	weaponCategory_ = category;
	weaponIdList_ = ids;

	// 新カテゴリで前回使ってた武器があれば優先、なければ先頭
	int32_t targetId = weaponIdList_.front();
	const int newIdx = static_cast<int>(weaponCategory_);
	if (newIdx >= 0 && newIdx < static_cast<int>(lastWeaponIdByCategory_.size()))
	{
		const int32_t last = lastWeaponIdByCategory_[newIdx];
		if (last > 0 && std::find(weaponIdList_.begin(), weaponIdList_.end(), last) != weaponIdList_.end())
			targetId = last;
	}

	if (!EquipWeaponByID(targetId))
	{
		if (weaponLoadError_.empty())
		{
			weaponLoadError_ = "WeaponMasterData: Equipに失敗しました。";
		}
		return;
	}
}

void PlayerWeaponComponent::ApplyMeleeInputRemap(InputSnapshot& snapshot)
{
	// Melee category: LMB attacks (disable gun fire)
	if (weaponCategory_ != EWeaponCategory::Melee) return;

	snapshot.meleePressed = snapshot.meleePressed || snapshot.firePressed;
	snapshot.fireHeld = false;
	snapshot.firePressed = false;
	snapshot.aimHeld = false;
	snapshot.aimPressed = false;
}

void PlayerWeaponComponent::BuildInitialAmmoViewCacheFromMasterData()
{
	ammoViewCache_ = {};
	ammoViewCacheValid_.fill(false);

	if (!weaponLoaded_) return;

	const auto& db = weaponSys_.Database();

	const EWeaponCategory categories[6] = {
		EWeaponCategory::Primary,
		EWeaponCategory::Backup,
		EWeaponCategory::Melee,
		EWeaponCategory::Special,
		EWeaponCategory::Sniper,
		EWeaponCategory::Heavy
	};

	for (EWeaponCategory cat : categories)
	{
		const int slot = CategoryToHotbarIndex(cat);
		if (slot < 0 || slot >= 6) continue;

		const auto ids = weaponSys_.GetWeaponIdListSortedByCategory(cat);
		if (ids.empty())
		{
			// このカテゴリに武器が無いなら非表示のまま
			continue;
		}

		// そのカテゴリで前回使っていた武器IDがあれば優先、なければ先頭
		int32_t targetId = ids.front();
		const int32_t lastId = lastWeaponIdByCategory_[slot];
		if (lastId > 0 &&
			std::find(ids.begin(), ids.end(), lastId) != ids.end())
		{
			targetId = lastId;
		}

		const FWeaponMasterData* data = db.FindByID(targetId);
		if (!data) continue;

		AmmoView v{};

		// 近接 or ammoなし は弾数非表示
		const bool isMelee = (data->coreData.category == EWeaponCategory::Melee);
		const bool noAmmo = (data->stats.ammoType == EAmmoType::None);

		if (isMelee || noAmmo)
		{
			v.usesAmmo = false;
		}
		else
		{
			v.usesAmmo = true;
			v.mag = std::max(0, data->stats.capacity);
			v.reserve = std::max(0, data->stats.maxReserveAmmo);
		}

		ammoViewCache_[slot] = v;
		ammoViewCacheValid_[slot] = true;
	}
}

int PlayerWeaponComponent::AddReserveAmmo(int amount)
{
	if (!weaponLoaded_) return 0;
	if (amount <= 0) return 0;
	if (weaponCategory_ == EWeaponCategory::Melee) return 0;

	const int restored = weaponSys_.Weapon().AddReserveAmmo(amount);
	// 予備弾薬が満タンで増えない場合も、HUD診断用キャッシュを現在値へ同期する。
	UpdateSelectedAmmoViewCache();
	return restored;
}

int PlayerWeaponComponent::GetMagazineAmmo() const
{
	if (!weaponLoaded_) return 0;
	return weaponSys_.Weapon().GetMagazineAmmo();
}

int PlayerWeaponComponent::GetReserveAmmo() const
{
	if (!weaponLoaded_) return 0;
	return weaponSys_.Weapon().GetReserveAmmo();
}

int PlayerWeaponComponent::GetMaxReserveAmmo() const
{
	if (!weaponLoaded_) return 0;
	return std::max(0, weaponSys_.Weapon().GetMaxReserveAmmo());
}

bool PlayerWeaponComponent::AddCurrentWeaponAmmo(int amount)
{
	return AddReserveAmmo(amount) > 0;
}

void PlayerWeaponComponent::UpdateSelectedAmmoViewCache() const
{
	if (!weaponLoaded_) return;

	const int selected = GetSelectedHot_barIndex();
	if (selected < 0 || selected >= 6) return;

	// 近接は弾薬表示なし
	if (weaponCategory_ == EWeaponCategory::Melee)
	{
		ammoViewCache_[selected] = AmmoView{};
		ammoViewCache_[selected].usesAmmo = false;
		ammoViewCacheValid_[selected] = true;
		return;
	}

	const auto& s = weaponSys_.Weapon().State();

	AmmoView cur{};
	cur.usesAmmo = true;
	cur.mag = s.magAmmo;
	cur.reserve = s.reserveAmmo;

	ammoViewCache_[selected] = cur;
	ammoViewCacheValid_[selected] = true;
}

void PlayerWeaponComponent::SaveCurrentWeaponState()
{
	if (!weaponLoaded_) return;
	if (currentWeaponId_ <= 0) return;

	const auto& w = weaponSys_.Weapon();
	const auto& s = w.State();

	SavedWeaponState saved{};
	saved.valid = true;
	saved.magAmmo = s.magAmmo;
	saved.reserveAmmo = s.reserveAmmo;
	saved.fireModeAutomatic = w.IsAutomatic();

	savedWeaponStates_[currentWeaponId_] = saved;

	// HUDキャッシュも今の武器状態で更新しておく
	UpdateSelectedAmmoViewCache();
}

void PlayerWeaponComponent::RestoreWeaponState(int32_t weaponID)
{
	if (!weaponLoaded_) return;
	if (weaponID <= 0) return;

	auto it = savedWeaponStates_.find(weaponID);
	if (it == savedWeaponStates_.end()) return;
	if (!it->second.valid) return;

	auto& w = weaponSys_.Weapon();
	auto& s = w.StateMutable();

	s.magAmmo = std::max(0, it->second.magAmmo);
	s.reserveAmmo = std::max(0, it->second.reserveAmmo);

	// 武器切替時はリロード中断・ADS解除・一時状態リセット
	s.isReloading = false;
	s.reloadRequested = false;
	s.reloadRequest = false;
	s.pendingReload = false;
	s.reloadTimer = 0.0f;
	s.isADS = false;

	// 発射モード復元
	const bool wantAuto = it->second.fireModeAutomatic;
	if (w.IsAutomatic() != wantAuto)
	{
		w.ToggleFireMode();
	}
}

void PlayerWeaponComponent::UpdateAndHandleInput(float dt, InputSnapshot& snapshot)
{
	// ロードしていないなら、切替入力が来た時だけロードを試す
	if (!weaponLoaded_)
	{
		if (snapshot.weaponSlotPressed != 0 ||
			snapshot.weaponSwitch != 0 ||
			snapshot.toggleFireModePressed)
		{
			LoadWeaponMasterDataOnce();
		}
	}

	// ---- Fire mode toggle ----
	if (weaponLoaded_ && snapshot.toggleFireModePressed)
	{
		weaponSys_.Weapon().ToggleFireMode();
	}

	// ------------------------------------------------------------
	// 先にカテゴリ切替
	// ------------------------------------------------------------
	if (snapshot.weaponSlotPressed != 0)
	{
		EWeaponCategory cat = weaponCategory_;
		if (SlotToCategory(snapshot.weaponSlotPressed, cat))
		{
			if (cat != weaponCategory_)
			{
				SwitchWeaponCategory(cat);
			}
		}
	}

	// ------------------------------------------------------------
	// 次に同カテゴリ内切替
	// ------------------------------------------------------------
	if (snapshot.weaponSwitch != 0)
	{
		SwitchWeaponByDelta(snapshot.weaponSwitch);
	}

	// ------------------------------------------------------------
	// 武器カテゴリ確定後に近接入力リマップ
	// ------------------------------------------------------------
	ApplyMeleeInputRemap(snapshot);

	lastAimHeld_ = snapshot.aimHeld;

	// 最後に武器内部更新
	TickWeapon(dt);

	// 弾切れ時の自動リロード
	TryAutoReload();
}

bool PlayerWeaponComponent::GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec) const
{
	if (!weaponLoaded_) { outIsReloading = false; outReloadTimer = 0.0f; outReloadSec = 0.0f; return false; }

	const auto& w = weaponSys_.Weapon();
	const auto& s = w.State();

	outIsReloading = s.isReloading;
	outReloadTimer = s.reloadTimer;
	outReloadSec = w.GetCurrentReloadDurationSec(); // ✅ ここ変更
	return true;
}

bool PlayerWeaponComponent::ShouldShowNoAmmoUI() const
{
	if (!weaponLoaded_) return false;
	if (weaponCategory_ == EWeaponCategory::Melee) return false;

	const auto& w = weaponSys_.Weapon();
	const auto& p = w.Params();
	const auto& s = w.State();

	// 弾を使わない武器は対象外
	if (p.ammoPerShot <= 0) return false;

	// リロード中は「NO AMMO」ではなくリロード表示を優先
	if (s.isReloading) return false;

	// マガジン不足 ＆ 予備弾ゼロ
	if (s.magAmmo < p.ammoPerShot && s.reserveAmmo <= 0)
	{
		return true;
	}

	return false;
}

bool PlayerWeaponComponent::CanFire(const InputSnapshot& snapshot) const
{
	if (!weaponLoaded_) return false;
	const auto& w = weaponSys_.Weapon();
	const auto& p = w.Params();
	const auto& s = w.State();

	if (s.isReloading) return false;
	if (s.fireCooldown > 0.0f) return false;
	if (s.magAmmo < p.ammoPerShot) return false;
	if (s.burstRemaining > 0 && s.burstTimer > 0.0f) return false;

	// 入力ゲート（発射モードに応じて判定）
	const bool autoMode = w.IsAutomatic();
	if (autoMode)
	{
		if (!snapshot.fireHeld) return false;
	}
	else
	{
		if (!snapshot.firePressed) return false;
	}

	return true;
}

bool PlayerWeaponComponent::TryFire(const InputSnapshot& snapshot,
	Ken4lowEngine::Camera* shootCamera,
	BulletManager* bulletManager,
	CollisionManager* collisionManager)
{
	if (!weaponLoaded_) return false;
	if (!bulletManager || !shootCamera) return false;

	auto& weapon = weaponSys_.Weapon();

	// ADS状態を毎フレーム反映
	weapon.SetADS(snapshot.aimHeld);

	const auto before = weapon.State();

	weapon.TryFire(snapshot.fireHeld, snapshot.firePressed, shootCamera, bulletManager, collisionManager);

	const auto& after = weapon.State();
	const bool fired = (after.magAmmo != before.magAmmo) ||
		(after.fireCooldown > before.fireCooldown) ||
		(after.burstRemaining != before.burstRemaining);
	return fired;
}

bool PlayerWeaponComponent::IsReloadFinished() const
{
	if (!weaponLoaded_) return true;
	return !weaponSys_.Weapon().State().isReloading;
}

void PlayerWeaponComponent::StartReload()
{
	if (!weaponLoaded_) return;
	weaponSys_.Weapon().StartReload();
}

bool PlayerWeaponComponent::ShouldAutoReload() const
{
	if (!weaponLoaded_) return false;
	if (weaponCategory_ == EWeaponCategory::Melee) return false;

	const auto& w = weaponSys_.Weapon();
	const auto& p = w.Params();
	const auto& s = w.State();

	// すでにリロード中なら不要
	if (s.isReloading) return false;

	// 1発でも残っていれば不要
	if (s.magAmmo > 0) return false;

	// 予備弾が無ければできない
	if (s.reserveAmmo <= 0) return false;

	// そもそも弾を使わない武器なら不要
	if (p.ammoPerShot <= 0) return false;

	// マガジン容量0みたいな特殊ケースも避ける
	if (p.magCapacity <= 0) return false;

	return true;
}

bool PlayerWeaponComponent::TryAutoReload()
{
	if (!ShouldAutoReload())
	{
		return false;
	}

	weaponSys_.Weapon().StartReload();
	return true;
}

void PlayerWeaponComponent::CancelReload()
{
	return; // 現状はリロードキャンセル不可
}

void PlayerWeaponComponent::AbortReload()
{
	// 現状は「強制中断」＝「キャンセル」と同じ扱いでOK。
	// 将来、死亡/スタン/武器破棄などで強制的に止めたい時に差を付けられる。
	CancelReload();
}

void PlayerWeaponComponent::StopReload()
{
	// 一般用途では止めない
	// どうしても中断したい場所は AbortReload / CancelReloadForced を使う
	return;
}

void PlayerWeaponComponent::CancelReloadForced()
{
	if (!weaponLoaded_) return;

	auto& w = weaponSys_.Weapon();
	if constexpr (requires(decltype(w) & ww) { ww.CancelReload(); })
	{
		w.CancelReload();
		return;
	}
}

void PlayerWeaponComponent::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("Weapon", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::Text("MasterDir: %s", weaponMasterDir_.string().c_str());

	if (!weaponLoaded_)
	{
		ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "WeaponMasterData: NOT LOADED");
		if (!weaponLoadError_.empty())
			ImGui::TextWrapped("%s", weaponLoadError_.c_str());

		if (ImGui::Button("Load WeaponMasterData"))
			LoadWeaponMasterDataOnce();
		return;
	}

	const auto& w = weaponSys_.Weapon();
	const auto& p = w.Params();
	const auto& s = w.State();

	ImGui::Text("WeaponID: %d", currentWeaponId_);
	ImGui::Text("DefaultAutomatic: %s", p.isAutomatic ? "true" : "false");
	ImGui::Text("CurrentFireMode: %s", w.IsAutomatic() ? "auto" : "semi");
	ImGui::Text("CanToggleMode: %s", p.canToggleFireMode ? "true" : "false");
	ImGui::Text("Damage: %.2f", p.damage);
	ImGui::Text("ProjectileSpeed: %.2f", p.projectileSpeed);
	ImGui::Text("SecPerShot: %.3fs", p.secPerShot);
	ImGui::Text("ReloadSec: %.2fs", p.reloadSec);

	ImGui::Separator();
	ImGui::Text("現在マガジン弾数: %d / %d", s.magAmmo, p.magCapacity);
	ImGui::Text("予備弾薬: %d", s.reserveAmmo);
	ImGui::Text("最大予備弾薬: %d", std::max(0, p.maxReserveAmmo));
	ImGui::Text("Reloading: %s (%.2fs)", s.isReloading ? "true" : "false", s.reloadTimer);
	ImGui::Text("Cooldown: %.3fs", s.fireCooldown);
	ImGui::Text("Burst: rem=%d  timer=%.3fs", s.burstRemaining, s.burstTimer);
	ImGui::Text("Spread: %.3f", s.spread);

	static int sEquipID = 0;
	ImGui::InputInt("EquipID", &sEquipID);
	if (ImGui::Button("Equip"))
		EquipWeaponByID(sEquipID);

	ImGui::SameLine();
	if (ImGui::Button("Reload WeaponMasterData"))
	{
		weaponLoaded_ = false;
		weaponLoadError_.clear();
		LoadWeaponMasterDataOnce();
	}
#endif
}

bool PlayerWeaponComponent::ReloadWeaponMasterDataAndReequip()
{
	if (!weaponLoaded_)
	{
		return LoadWeaponMasterDataOnce();
	}

	std::string err;
	if (!weaponSys_.ReloadAndReequip(&err))
	{
		weaponLoadError_ = err;
		return false;
	}

	// 現カテゴリの一覧を更新
	weaponIdList_ = weaponSys_.GetWeaponIdListSortedByCategory(weaponCategory_);
	if (weaponIdList_.empty())
	{
		weaponIdList_ = weaponSys_.GetWeaponIdListSorted();
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
	return true;
}

bool PlayerWeaponComponent::RebuildCurrentWeaponFromDatabase()
{
	if (!weaponLoaded_)
	{
		return LoadWeaponMasterDataOnce();
	}

	std::string err;
	bool ok = false;

	if (currentWeaponId_ > 0)
	{
		ok = weaponSys_.EquipById(currentWeaponId_, &err);
	}
	else
	{
		ok = weaponSys_.EquipFirst(&err);
	}

	if (!ok)
	{
		weaponLoadError_ = err;
		return false;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
	return true;
}