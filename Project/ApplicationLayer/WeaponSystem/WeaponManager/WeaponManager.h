#pragma once
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
#include <unordered_map>
#include <WeaponConfig.h>
#include <Vector3.h>
#include <utility>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }
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

public: /// ---------- 構造体 ---------- ///

	// 弾薬表示用構造体
	struct AmmoView
	{
		int mag = 0;			// 現在のマガジン
		int reserve = 0;		// 予備弾
		int magSize = 0;		// マガジン最大
		int reserveMax = 0;		// 予備弾最大
		bool reloading = false;	// リロード中かどうか
		float reloadT = 0.0f;	// リロード経過時間
		float reloadSec = 0.0f;	// リロード所要時間
		bool usesAmmo = false;	// 弾薬を使用する武器かどうか
	};

private: /// ---------- 構造体 ---------- ///

	// 内部用：弾薬パラメータ構造体
	struct AmmoParams
	{
		int magSize = 0;
		int reserveMax = 0;
		int consumePerShot = 1;
		float reloadSec = 1.5f;
		bool usesAmmo = true;     // melee等はfalse
		bool infinite = false;    // デバッグ用

		bool autoReload = true;
	};

	// 内部用：弾薬状態構造体
	struct AmmoState
	{
		AmmoParams p{};
		int mag = 0;
		int reserve = 0;
		bool reloading = false;
		float t = 0.0f;
	};

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
	bool StartFireBallisticEffect(const K4E::Vector3& position, const K4E::Vector3& velocity);

	// プレイヤーのボディを設定
	void SetPlayerBody(const K4E::WorldTransformEx* bodyTransform);

	// 親を設定
	void SetParentTransforms(const K4E::WorldTransformEx* rightArmTransform);

	// 現在の武器設定を取得
	const WeaponConfig& GetCurrentConfig() const { return fireState_.weaponConfig; }

	// 衝突管理者を設定
	void SetCollisionManager(CollisionManager* collisionManager) { ballisticEffect_->SetCollisionManager(collisionManager); }

	// 衝突コライダー登録
	void RegisterColliders(CollisionManager* mgr);

	// 銃口のワールド座標を取得
	K4E::Vector3 GetMuzzleWorld() const { return ballisticEffect_ ? ballisticEffect_->GetMuzzleWorld() : K4E::Vector3{}; }

	// 0..5 を返す（Primary..Heavy）。未選択なら -1
	int GetSelectedHot_barIndex() const;

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 現在選択中のスロットの弾薬情報を取得します。
	/// </summary>
	/// <returns>弾薬情報構造体。</returns>
	AmmoView GetCurrentAmmoView() const;

	// スロット(0..5)の弾薬情報を取得（空/近接は usesAmmo=false）
	AmmoView GetAmmoViewByHot_barIndex(int hotbarIndex) const;

private: /// ---------- メンバ関数 ---------- ///

	// 武器選択
	void SelectWeapon(const std::string& name);

	void BuildDefaultAmmo();
	AmmoState* GetAmmo(const std::string& weaponName);
	const AmmoState* GetAmmo(const std::string& weaponName) const;
	AmmoState* GetCurrentAmmo();
	const AmmoState* GetCurrentAmmo() const;

	void StartReload();
	void UpdateReload(float dt);

	bool TryConsumeAmmoForShot(); // 撃てるなら消費してtrue

private: /// ---------- メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力クラス
	CollisionManager* collisionManager_ = nullptr; // 衝突管理者

	// 親ワールド変換ポインタ
	const K4E::WorldTransformEx* rightArmTransform_ = nullptr;

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

	// 武器ごとの弾薬状態管理用マップ
	std::unordered_map<std::string, AmmoState> ammoByWeapon_;

private: /// ---------- 武器データパス定数 ---------- ///

	const std::string kWeaponDir = "Resources/JSON/weapons";		   // 武器データディレクトリ
	const std::string kWeaponMonolith = "Resources/JSON/weapons.json"; // 武器データモノリス
};

