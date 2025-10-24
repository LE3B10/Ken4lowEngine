#include "WeaponManager.h"
#include <Input.h>
#include <ToWeaponConfig.h>

#include <ImGuiManager.h>

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
	input_ = Input::GetInstance();

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
	// 武器選択 : 数字キー1〜6 : クラス別選択
	if (input_->TriggerKey(DIK_1)) { auto n = loadout_->SelectNameByClass(WeaponClass::Primary, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_2)) { auto n = loadout_->SelectNameByClass(WeaponClass::Backup, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_3)) { auto n = loadout_->SelectNameByClass(WeaponClass::Melee, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_4)) { auto n = loadout_->SelectNameByClass(WeaponClass::Special, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_5)) { auto n = loadout_->SelectNameByClass(WeaponClass::Sniper, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }
	if (input_->TriggerKey(DIK_6)) { auto n = loadout_->SelectNameByClass(WeaponClass::Heavy, weaponCatalog_->All()); if (!n.empty()) SelectWeapon(n); }

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
	if (!deathState_.isDead && 0 <= currentIndex_ && currentIndex_ < static_cast<int>(weapons_.size()))
	{
		// 武器描画
		weapons_[currentIndex_]->Draw();
	}

	// 弾道エフェクト描画
	if (!deathState_.isDead) ballisticEffect_->Draw();
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

/// -------------------------------------------------------------
///				　	 弾道エフェクト開始処理
/// -------------------------------------------------------------
void WeaponManager::StartFireBallisticEffect(const Vector3& position, const Vector3& velocity)
{
	ballisticEffect_->Start(position, velocity, fireState_.weaponConfig);
}

/// -------------------------------------------------------------
///				　	 親ワールド変換設定処理
/// -------------------------------------------------------------
void WeaponManager::SetParentTransforms(const WorldTransformEx* rightArmTransform)
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

/// -------------------------------------------------------------
///				　		ImGui武器の描画処理
/// -------------------------------------------------------------
void WeaponManager::DrawWeaponImGui()
{
	// 現在装備の名前（空可）
	const std::string currentName = weapon_ ? weapon_->Data().name : std::string{};

	// ---------- ロードアウト表示 ---------- ///
	static const char* kClassLabels[] = { "Primary","Backup","Melee","Special","Sniper","Heavy" };

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
		int idx = static_cast<int>(D.clazz);
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
}
