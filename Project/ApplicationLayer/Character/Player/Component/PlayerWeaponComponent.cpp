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
}

void PlayerWeaponComponent::SetMasterDirectory(const std::filesystem::path& dir)
{
	weaponMasterDir_ = dir;
	weaponLoaded_ = false;
	weaponLoadError_.clear();
	weaponIdList_.clear();
	currentWeaponId_ = 0;
	weaponSys_ = WeaponSystem{};
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

void PlayerWeaponComponent::TickWeapon(float dt)
{
	if (!weaponLoaded_) return;
	weaponSys_.Tick(dt);
}

bool PlayerWeaponComponent::EquipWeaponByID(int32_t weaponID)
{
	if (!weaponLoaded_) LoadWeaponMasterDataOnce();
	if (!weaponLoaded_) return false;

	std::string err;
	if (!weaponSys_.EquipById(weaponID, &err))
	{
		weaponLoadError_ = err;
		return false;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
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

	std::string err;
	if (!weaponSys_.EquipById(targetId, &err))
	{
		weaponLoadError_ = err.empty() ? "WeaponMasterData: Equipに失敗しました。" : err;
		return;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
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

void PlayerWeaponComponent::UpdateAndHandleInput(float dt, InputSnapshot& snapshot)
{
	// ロードしてないなら、入力での切替などは無視できるようにする
	if (!weaponLoaded_)
	{
		// 明示ロードボタン/初期化でロードする想定だが、保険でトグル等が来ても落ちない
		// ただし、切替キーが押されたらロードを試みるのは便利なのでやっておく
		if (snapshot.weaponSlotPressed != 0 || snapshot.weaponSwitch != 0 || snapshot.toggleFireModePressed)
			LoadWeaponMasterDataOnce();
	}

	// ---- Fire mode toggle (V) ----
	if (weaponLoaded_ && snapshot.toggleFireModePressed)
	{
		weaponSys_.Weapon().ToggleFireMode();
	}

	// ---- 近接カテゴリ時の入力リマップ ----
	ApplyMeleeInputRemap(snapshot);

	// Weapon（クールダウン/リロード/バースト/拡散）
	TickWeapon(dt);

	// カテゴリ切替（数字キー1..6）
	if (snapshot.weaponSlotPressed != 0)
	{
		EWeaponCategory cat = weaponCategory_;
		if (SlotToCategory(snapshot.weaponSlotPressed, cat))
		{
			if (cat != weaponCategory_)
				SwitchWeaponCategory(cat);
		}
	}

	// 武器切替（ホイール/DPAD想定）
	if (snapshot.weaponSwitch != 0)
	{
		SwitchWeaponByDelta(snapshot.weaponSwitch);
	}
}

bool PlayerWeaponComponent::GetReloadUI(bool& outIsReloading, float& outReloadTimer, float& outReloadSec) const
{
	if (!weaponLoaded_) { outIsReloading = false; outReloadTimer = 0.0f; outReloadSec = 0.0f; return false; }
	const auto& w = weaponSys_.Weapon();
	const auto& s = w.State();
	const auto& p = w.Params();
	outIsReloading = s.isReloading;
	outReloadTimer = s.reloadTimer;
	outReloadSec = p.reloadSec;
	return true;
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

	// 重要: State() は参照返しなので、参照のまま before を取ると
	// TryFire() 後に "before" まで更新後の値になってしまう（同じ実体を見ている）。
	// → 発射判定が常に false になり、Player 側の recoil が発動しない原因になる。
	const auto before = weaponSys_.Weapon().State(); // 値コピーでスナップショット化

	weaponSys_.Weapon().TryFire(snapshot.fireHeld, snapshot.firePressed, shootCamera, bulletManager, collisionManager);

	const auto& after = weaponSys_.Weapon().State();
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

void PlayerWeaponComponent::CancelReload()
{
	if (!weaponLoaded_) return;

	auto& w = weaponSys_.Weapon();
	if constexpr (requires(decltype(w) & ww) { ww.CancelReload(); })
	{
		w.CancelReload();
		return;
	}
}

void PlayerWeaponComponent::AbortReload()
{
	// 現状は「強制中断」＝「キャンセル」と同じ扱いでOK。
	// 将来、死亡/スタン/武器破棄などで強制的に止めたい時に差を付けられる。
	CancelReload();
}

void PlayerWeaponComponent::StopReload()
{
	// 別名（CancelReload/AbortReload を統一して呼びたい時用）
	CancelReload();
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
	ImGui::Text("Ammo: %d / %d   (Reserve: %d)", s.magAmmo, p.magCapacity, s.reserveAmmo);
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
