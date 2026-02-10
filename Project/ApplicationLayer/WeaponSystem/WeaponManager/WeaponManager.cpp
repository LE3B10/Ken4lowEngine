#define NOMINMAX
#include "WeaponManager.h"
#include <Input.h>
#include <ToWeaponConfig.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#include <algorithm>
#include <WorldTransformEx.h>
#include <WeaponEditorUI.h>
#include <WeaponData.h>
#include <WeaponConfig.h>
#include <WeaponClass.h>
#include <WeaponCatalog.h>
#include <Vector3.h>
#include <Weapon.h>
#include <string>
#include <PistolWeapon.h>
#include <memory>
#include <Loadout.h>
#include <dinput.h>
#include <BaseWeapon.h>
#include <BallisticEffect.h>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　	武器名からインデックスを取得
/// -------------------------------------------------------------
int WeaponManager::FindIndexByName(const std::string& name) const
{
	// 武器カタログがなければ無効
	if (!weaponCatalog_) return -1;

	// カタログ順と weapons_ の構築順を一致させている前提
	int i = 0;
	for (auto& [n, _] : weaponCatalog_->All())
	{
		// 名前が一致したらインデックスを返す
		if (n == name) return i;

		// インデックス加算
		++i;
	}

	// 見つからなかった
	return -1;
}

/// -------------------------------------------------------------
///				　		  武器生成処理
/// -------------------------------------------------------------
std::unique_ptr<BaseWeapon> WeaponManager::CreateWeaponFromConfig(const WeaponConfig& config) const
{
	// ピストル
	if (config.name == "Pistol")
	{
		std::unique_ptr<PistolWeapon> weapon = std::make_unique<PistolWeapon>();
		weapon->Initialize();
		weapon->SetParentTransform(rightArmTransform_); // 右腕に追従
		return weapon;
	}

	// 未対応タイプは必ずフォールバックを返す
	auto fallback = std::make_unique<PistolWeapon>();
	fallback->Initialize();
	fallback->SetParentTransform(rightArmTransform_);
	return fallback;
}

/// -------------------------------------------------------------
///				　	　	武器の初期化処理
/// -------------------------------------------------------------
void WeaponManager::InitializeWeapons(const FireState& fireState, const DeathState& deathState)
{
	input_ = K4E::Input::GetInstance();

	fireState_ = fireState; // 射撃状態構造体コピー
	deathState_ = deathState; // 死亡状態構造体コピー

	// 弾道エフェクト初期化
	ballisticEffect_ = std::make_unique<BallisticEffect>();
	ballisticEffect_->Initialize();
	ballisticEffect_->SetParentTransform(rightArmTransform_); // 右腕に追従

	// 武器カタログ初期化
	weaponCatalog_ = std::make_unique<WeaponCatalog>();
	weaponCatalog_->Initialize(kWeaponDir, kWeaponMonolith); // ディレクトリとモノリス両方から読み込み

	// ロードアウト初期化
	loadout_ = std::make_unique<Loadout>();
	loadout_->Rebuild(weaponCatalog_->All()); // 在庫に基づき再構築

	// 弾の初期値を武器ごとに作る
	BuildDefaultAmmo();

	// 武器リストクリア
	weapons_.clear();

	// 武器リストに在庫の武器を追加
	for (auto& [name, data] : weaponCatalog_->All())
	{
		WeaponConfig cfg = ToWeaponConfig(data);
		weapons_.push_back(CreateWeaponFromConfig(cfg));
	}

	// 初期装備 : プライマリ武器を優先
	std::string useWeaponName = loadout_->SelectNameByClass(WeaponClass::Primary, weaponCatalog_->All());

	// 装備がなければ在庫の最初の武器を使う
	if (useWeaponName.empty() && !weaponCatalog_->All().empty()) useWeaponName = weaponCatalog_->All().begin()->first;

	// 武器選択
	if (!useWeaponName.empty()) SelectWeapon(useWeaponName);
}

/// -------------------------------------------------------------
///				　	　	武器の更新処理
/// -------------------------------------------------------------
void WeaponManager::UpdateWeapons(float deltaTime)
{
	// リロード進行（先に進めておくと気持ちいい）
	UpdateReload(deltaTime);

	// Rでリロード開始
	if (input_->TriggerKey(DIK_R))
	{
		StartReload();
	}

	// 武器選択 : 数字キー1〜6 : クラス別選択
	if (input_->TriggerKey(DIK_1)) { auto n = loadout_->SelectNameByClass(WeaponClass::Primary, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_2)) { auto n = loadout_->SelectNameByClass(WeaponClass::Backup, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_3)) { auto n = loadout_->SelectNameByClass(WeaponClass::Melee, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_4)) { auto n = loadout_->SelectNameByClass(WeaponClass::Special, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_5)) { auto n = loadout_->SelectNameByClass(WeaponClass::Sniper, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_6)) { auto n = loadout_->SelectNameByClass(WeaponClass::Heavy, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }

	// --- マウスホイールで武器クラスを循環選択（1～6相当） ---
	if (input_ && loadout_ && weaponCatalog_)
	{
		const int wheel = input_->GetMouseWheel(); // DIMOUSESTATE.lZ（だいたい ±120 で1ノッチ）
		if (wheel != 0)
		{
			const int dir = (wheel > 0) ? -1 : +1;

			// 速く回した時に ±240 などが来るので段数をざっくり反映（最大6回まで）
			int mag = (wheel >= 0) ? wheel : -wheel;
			int steps = mag / 120;
			if (steps <= 0) steps = 1;
			if (steps > 6)  steps = 6;

			for (int s = 0; s < steps; ++s)
			{
				int idx = GetSelectedHot_barIndex();
				if (idx < 0) idx = 0;

				// 空のクラスを飛ばして次の装備を探す（最大6回で打ち切り）
				for (int tries = 0; tries < 6; ++tries)
				{
					idx = (idx + dir + 6) % 6;

					auto name = loadout_->SelectNameByClass(static_cast<WeaponClass>(idx), weaponCatalog_->All());
					if (!name.empty())
					{
						SelectWeapon(name);
						break;
					}
				}
			}
		}
	}


	// 現在装備の武器インデックスを取得
	if (0 <= currentIndex_ && currentIndex_ < static_cast<int>(weapons_.size()))
	{
		// 武器更新
		weapons_[currentIndex_]->Update(deltaTime);
	}

	// 弾道エフェクト更新
	ballisticEffect_->Update();
}

/// -------------------------------------------------------------
///				　		　武器の描画処理
/// -------------------------------------------------------------
void WeaponManager::DrawWeapons()
{
	// 現在装備の武器インデックスを取得
	//if (!deathState_.isDead && 0 <= currentIndex_ && currentIndex_ < static_cast<int>(weapons_.size()))
	//{
		// 武器描画
	weapons_[currentIndex_]->Draw();
	//}

	// 弾道エフェクト描画
	if (!deathState_.isDead) ballisticEffect_->Draw();
}

int WeaponManager::GetSelectedHot_barIndex() const
{
	if (!weapon_) return -1;
	// WeaponData::weapon_class は Primary..Heavy(0..5) になってる前提（Loadout.cppの並び）
	return static_cast<int>(weapon_->Data().weapon_class);
}

WeaponManager::AmmoView WeaponManager::GetCurrentAmmoView() const
{
	AmmoView v{};
	const AmmoState* st = GetCurrentAmmo();
	if (!st) return v;

	v.mag = st->mag;
	v.reserve = st->reserve;
	v.magSize = st->p.magSize;
	v.reserveMax = st->p.reserveMax;
	v.reloading = st->reloading;
	v.reloadT = st->t;
	v.reloadSec = st->p.reloadSec;
	v.usesAmmo = st->p.usesAmmo;
	return v;
}

WeaponManager::AmmoView WeaponManager::GetAmmoViewByHot_barIndex(int hot_barIndex) const
{
	AmmoView v{};
	if (hot_barIndex < 0 || hot_barIndex >= 6) return v;
	if (!loadout_) return v;

	const auto& map = loadout_->GetEquipMap();
	auto it = map.find(static_cast<WeaponClass>(hot_barIndex));
	if (it == map.end()) return v;

	const std::string& weaponName = it->second;
	if (weaponName.empty()) return v;

	const AmmoState* st = GetAmmo(weaponName);
	if (!st) return v;

	v.mag = st->mag;
	v.reserve = st->reserve;
	v.magSize = st->p.magSize;
	v.reserveMax = st->p.reserveMax;
	v.reloading = st->reloading;
	v.reloadT = st->t;
	v.reloadSec = st->p.reloadSec;
	v.usesAmmo = st->p.usesAmmo;
	return v;
}

/// -------------------------------------------------------------
///				　			　 武器選択
/// -------------------------------------------------------------
void WeaponManager::SelectWeapon(const std::string& name)
{
	// 武器データをカタログから探す
	if (const WeaponData* w = weaponCatalog_->Find(name))
	{
		// 武器基底ポインタにセット
		weapon_ = std::make_unique<Weapon>(*w);

		// ランタイム用コピー
		fireState_.weaponConfig = ToWeaponConfig(*w);

		// 武器リストからインデックスを探す
		currentIndex_ = FindIndexByName(name);
	}
}

void WeaponManager::BuildDefaultAmmo()
{
	ammoByWeapon_.clear();
	if (!weaponCatalog_) return;

	for (auto& [name, data] : weaponCatalog_->All())
	{
		AmmoState st{};

		// 近接は弾薬を使わない（表示もしない）
		st.p.usesAmmo = (data.weapon_class != WeaponClass::Melee);
		st.p.infinite = false;

		// JSON反映（WeaponData.h の項目）
		st.p.magSize = std::max(0, data.magCapacity);
		st.p.reserveMax = std::max(0, data.startingReserve);
		st.p.reloadSec = std::max(0.0f, data.reloadTime);
		st.p.autoReload = data.autoReload;

		// 1発で消費する弾数：今は 1 のまま（JSONに別項目が無いので）
		st.p.consumePerShot = 1;

		// usesAmmo=false の武器は 0固定
		if (!st.p.usesAmmo)
		{
			st.p.magSize = 0;
			st.p.reserveMax = 0;
			st.p.reloadSec = 0.0f;
			st.p.consumePerShot = 0;
		}

		// 初期値：マガジン満タン + 予備満タン
		st.mag = st.p.usesAmmo ? st.p.magSize : 0;
		st.reserve = st.p.usesAmmo ? st.p.reserveMax : 0;

		ammoByWeapon_[name] = st;
	}
}

WeaponManager::AmmoState* WeaponManager::GetAmmo(const std::string& weaponName)
{
	auto it = ammoByWeapon_.find(weaponName);
	return (it == ammoByWeapon_.end()) ? nullptr : &it->second;
}

const WeaponManager::AmmoState* WeaponManager::GetAmmo(const std::string& weaponName) const
{
	auto it = ammoByWeapon_.find(weaponName);
	return (it == ammoByWeapon_.end()) ? nullptr : &it->second;
}

WeaponManager::AmmoState* WeaponManager::GetCurrentAmmo()
{
	if (!weapon_) return nullptr;
	return GetAmmo(weapon_->Data().name);
}

const WeaponManager::AmmoState* WeaponManager::GetCurrentAmmo() const
{
	if (!weapon_) return nullptr;
	return GetAmmo(weapon_->Data().name);
}

void WeaponManager::StartReload()
{
	AmmoState* st = GetCurrentAmmo();
	if (!st) return;
	if (!st->p.usesAmmo || st->p.infinite) return;
	if (st->reloading) return;
	if (st->mag >= st->p.magSize) return;
	if (st->reserve <= 0) return;

	st->reloading = true;
	st->t = 0.0f;
}

void WeaponManager::UpdateReload(float dt)
{
	AmmoState* st = GetCurrentAmmo();
	if (!st) return;
	if (!st->reloading) return;

	st->t += dt;
	if (st->t < st->p.reloadSec) return;

	// リロード完了
	const int need = st->p.magSize - st->mag;
	const int load = std::min(need, st->reserve);

	st->mag += load;
	st->reserve -= load;

	st->reloading = false;
	st->t = 0.0f;
}

bool WeaponManager::TryConsumeAmmoForShot()
{
	AmmoState* st = GetCurrentAmmo();
	if (!st) return true;                 // 未設定は無限扱い
	if (!st->p.usesAmmo || st->p.infinite) return true;
	if (st->reloading) return false;

	if (st->mag >= st->p.consumePerShot)
	{
		st->mag -= st->p.consumePerShot;
		return true;
	}

	// 弾切れ：autoReload が true のときだけ自動リロード
	if (st->reserve > 0 && st->p.autoReload) StartReload();
	return false;
}

/// -------------------------------------------------------------
///				　	 弾道エフェクト開始処理
/// -------------------------------------------------------------
bool WeaponManager::StartFireBallisticEffect(const K4E::Vector3& position, const K4E::Vector3& velocity)
{
	// 弾が無いなら発射しない
	if (!TryConsumeAmmoForShot()) return false;

	// 弾道エフェクト開始
	ballisticEffect_->Start(position, velocity, fireState_.weaponConfig);

	return true;
}

/// -------------------------------------------------------------
///				　	　 プレイヤーボディ設定処理
/// -------------------------------------------------------------
void WeaponManager::SetPlayerBody(const K4E::WorldTransformEx* bodyTransform)
{
	// プレイヤーボディTransformを保存
	if (ballisticEffect_) ballisticEffect_->SetPlayerBodyTransform(*bodyTransform, {});
}

/// -------------------------------------------------------------
///				　	 親ワールド変換設定処理
/// -------------------------------------------------------------
void WeaponManager::SetParentTransforms(const K4E::WorldTransformEx* rightArmTransform)
{
	// 右腕Transformを保存
	rightArmTransform_ = rightArmTransform;
	if (ballisticEffect_) ballisticEffect_->SetParentTransform(rightArmTransform_);

	// 全武器に親Transformを設定
	for (auto& weapon : weapons_)
	{
		if (auto* pw = dynamic_cast<PistolWeapon*>(weapon.get()))
		{
			pw->SetParentTransform(rightArmTransform_);
		}
	}
}

void WeaponManager::RegisterColliders(CollisionManager* mgr)
{
	if (ballisticEffect_) ballisticEffect_->RegisterColliders(mgr);
}

/// -------------------------------------------------------------
///				　		ImGui武器の描画処理
/// -------------------------------------------------------------
void WeaponManager::DrawWeaponImGui()
{
#ifdef USE_IMGUI
	// 現在装備の名前（空可）
	const std::string currentName = weapon_ ? weapon_->Data().name : std::string{};

	// ---------- ロードアウト表示 ---------- ///
	static const char* kClassLabels[] = { "プライマリ","バックアップ","近接","特殊","スナイパー","ヘビー" };

	ImGui::Separator();
	ImGui::Text("Loadout by Class");

	const auto& map = loadout_->GetEquipMap();
	for (int c = 0; c < 6; ++c)
	{
		WeaponClass wc = static_cast<WeaponClass>(c);
		const char* label = kClassLabels[c];
		std::string equipped = "-";
		if (auto it = map.find(wc); it != map.end()) equipped = it->second;
		ImGui::Text("%-8s : %s", label, equipped.c_str());
	}

	// 現在装備中の武器カテゴリ表示
	if (weapon_)
	{
		const auto& D = weapon_->Data(); // 現在装備中の武器データ（const）
		int idx = static_cast<int>(D.weapon_class);
		if (0 <= idx && idx < IM_ARRAYSIZE(kClassLabels))
		{
			ImGui::Text("Current Category: %s", kClassLabels[idx]);
		}
		else
		{
			ImGui::Text("Current Category: (Unknown)");
		}
	}

	// --- WeaponEditorUI に渡すフック群 ---
	WeaponEditorHooks hooks{};

	// 全保存フック
	hooks.SaveAll = [&] { weaponCatalog_->SaveAll(); };

	// 再読込予約フック
	hooks.RequestReloadFocus = [&](const std::string& focus) { weaponCatalog_->RequestReload(focus); };

	// 装備再構築フック
	hooks.RebuildLoadout = [&] { loadout_->Rebuild(weaponCatalog_->All()); };

	// ランタイム反映フック
	hooks.ApplyToRuntimeIfCurrent = [&](const WeaponData& wd) {
		if (weapon_ && weapon_->Data().name == wd.name) {
			fireState_.weaponConfig = ToWeaponConfig(wd);                    // ランタイム反映
		}
		};

	// 追加フック
	hooks.RequestAdd = [&](const std::string& newName, const std::string& baseName) {
		pendingAdds_.emplace_back(newName, baseName);               // 追加はフレーム末で実行
		};

	// 削除フック
	hooks.RequestDelete = [&](const std::string& name) {
		pendingDeletes_.push_back(name);                            // 削除もフレーム末で
		};

	// --- メインの “武器編集UI” 呼び出し ---
	if (!weaponEditorUI_) weaponEditorUI_ = std::make_unique<WeaponEditorUI>();
	weaponEditorUI_->DrawImGui(*weaponCatalog_, currentName, hooks);

	// --- フレーム末の遅延実行（Add/Delete） ---
	for (auto& [newName, baseName] : pendingAdds_) {
		const WeaponData* basePtr = baseName.empty() ? nullptr : weaponCatalog_->Find(baseName);
		weaponCatalog_->AddWeapon(newName, basePtr);
	}
	pendingAdds_.clear();

	// --- 削除処理 ---
	for (auto& delName : pendingDeletes_)
	{
		loadout_->RemoveName(delName);
		weaponCatalog_->RemoveWeapon(delName);
	}

	// 削除リストクリア
	pendingDeletes_.clear();

	// --- “再読込予約”の適用（フォーカス再選択＆装備更新） ---
	weaponCatalog_->ApplyReloadIfNeeded([&](const WeaponData& focused) {
		weapon_ = std::make_unique<Weapon>(focused);
		fireState_.weaponConfig = ToWeaponConfig(focused);
		loadout_->Rebuild(weaponCatalog_->All());
		});
#endif // USE_IMGUI
}
