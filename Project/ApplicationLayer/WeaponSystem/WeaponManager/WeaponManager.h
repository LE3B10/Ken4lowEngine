#pragma once
#include "PistolWeapon.h"
#include "BallisticEffect.h"
#include "Weapon.h"
#include "WeaponCatalog.h"
#include "Loadout.h"
#include "WeaponEditorUI.h"
#include "WorldTransformEx.h"
#include "FireState.h"
#include "DeathState.h"
#include "BaseWeapon.h"

#include <vector>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Input;
class CollisionManager;

/// -------------------------------------------------------------
///				　		  武器管理クラス
/// -------------------------------------------------------------
class WeaponManager
{
	// 武器名からインデックスを取得
	int  FindIndexByName(const std::string& name) const;

	// 武器生成処理
	std::unique_ptr<BaseWeapon> CreateWeaponFromConfig(const WeaponConfig& config) const;

public: /// ---------- メンバ関数 ---------- ///

	// 武器の初期化
	void InitializeWeapons(const FireState& fireState, const DeathState& deathState);

	// 武器の更新
	void UpdateWeapons(float deltaTime);

	// 武器の描画
	void DrawWeapons();

	// 武器のImGui描画
	void DrawWeaponImGui();

	// 弾道エフェクト開始
	void StartFireBallisticEffect(const Vector3& position, const Vector3& velocity);

	// 親を設定
	void SetParentTransforms(const WorldTransformEx* rightArmTransform);

	// 現在の武器設定を取得
	const WeaponConfig& GetCurrentConfig() const { return fireState_.weaponConfig; }

	// 衝突管理者を設定
	void SetCollisionManager(CollisionManager* collisionManager) { ballisticEffect_->SetCollisionManager(collisionManager); }

	void RegisterColliders(CollisionManager* mgr);

private: /// ---------- メンバ関数 ---------- ///

	// 武器選択
	void SelectWeapon(const std::string& name);

private: /// ---------- メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力クラス
	CollisionManager* collisionManager_ = nullptr; // 衝突管理者

	// 親ワールド変換ポインタ
	const WorldTransformEx* rightArmTransform_ = nullptr;

	FireState fireState_; // 射撃状態構造体
	DeathState deathState_; // 死亡状態構造体

	// 武器リスト : 基底ポインタの配列
	std::vector<std::unique_ptr<BaseWeapon>> weapons_;

	// 武器データ読み込み用基底ポインタ
	std::unique_ptr<Weapon> weapon_;

	std::unique_ptr<BallisticEffect> ballisticEffect_; // 弾道エフェクト
	std::unique_ptr<WeaponCatalog> weaponCatalog_;  // 武器カタログ
	std::unique_ptr<Loadout> loadout_;				// ロードアウト

	std::unique_ptr<WeaponEditorUI> weaponEditorUI_; // 武器エディタUI

	// 遅延コマンド用のキュー（Add/Delete をフレーム末で実行する）
	std::vector<std::pair<std::string, std::string>> pendingAdds_;
	std::vector<std::string> pendingDeletes_; // 削除リスト

	// 武器ごとの「編集ウィンドウが開いているか」状態
	std::unordered_map<std::string, bool> weaponEditorOpen_;

	int currentIndex_ = -1; // 現在装備

private: /// ---------- 武器データパス定数 ---------- ///

	const std::string kWeaponDir = "Resources/JSON/weapons";		   // 武器データディレクトリ
	const std::string kWeaponMonolith = "Resources/JSON/weapons.json"; // 武器データモノリス
};

