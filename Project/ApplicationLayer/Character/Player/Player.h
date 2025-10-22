#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include <FpsCamera.h>

#include "PistolWeapon.h"
#include "BallisticEffect.h"
#include "Weapon.h"
#include "WeaponCatalog.h"
#include "Loadout.h"
#include "WeaponEditorUI.h"

#include <memory>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class Input;


/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~Player() = default;

	// 初期化処理
	void Initialize() override;

	// 更新処理
	void Update(float deltaTime) override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
	void DrawImGui() override;

	// ワールド変換の取得
	WorldTransformEx* GetWorldTransform() { return &body_.transform; }

	// 衝突時に呼ばれる仮想関数
	void OnCollision(Collider* other) override {};

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

public: /// ---------- アクセサー関数 ---------- ///

	// デバッグカメラフラグ取得
	bool IsDebugCamera() const { return isDebugCamera_; }
	void SetDebugCamera(bool isDebug) { isDebugCamera_ = isDebug; }

	// FPSカメラ取得
	FpsCamera* GetFpsCamera() const { return fpsCamera_.get(); }

	// プレイヤーモデル取得
	Object3D* GetPlayerModel() const { return body_.object.get(); }

private: /// ---------- メンバ関数 ---------- ///

	// 移動処理
	void Move(float deltaTime);

	// リコイル
	void ApplyRecoil(float kickBack, float riseDeg, float horizDeg);

	// 武器選択
	void SelectWeapon(const std::string& name);

private: /// ---------- デバッグカメラフラグ ---------- ///

	const std::string kWeaponDir = "Resources/JSON/weapons";		   // 武器データディレクトリ
	const std::string kWeaponMonolith = "Resources/JSON/weapons.json"; // 武器データモノリス

	Input* input_ = nullptr; // 入力クラス

	std::unique_ptr<FpsCamera> fpsCamera_; // FPSカメラ

	std::unique_ptr<PistolWeapon> pistolWeapon_; // ピストル武器
	std::unique_ptr<BallisticEffect> ballisticEffect_; // 弾道エフェクト

	std::unique_ptr<Weapon> weapon_; // 武器基底ポインタ

	std::unique_ptr<WeaponCatalog> weaponCatalog_; // 武器カタログ
	std::unique_ptr<Loadout> loadout_; // ロードアウト

	std::unique_ptr<WeaponEditorUI> weaponEditorUI_; // 武器エディタUI

	// 遅延コマンド用のキュー（Add/Delete をフレーム末で実行する）
	std::vector<std::pair<std::string, std::string>> pendingAdds_; // (newName, baseNameOrEmpty)
	std::vector<std::string> pendingDeletes_;

	// 武器ごとの「編集ウィンドウが開いているか」状態
	std::unordered_map<std::string, bool> weaponEditorOpen_;

	// 新規追加した武器のウィンドウを自動で開くか
	bool autoOpenEditorOnAdd_ = true;

	float bodyYaw_ = 0.0f;        // 体の現在Yaw（ラジアン）
	float headYawLocal_ = 0.0f;   // 頭のローカルYaw（親=体に対する差）

	Vector3 rightArmPosition_ = { 1.5f, 1.5f, 0 }; // 右腕の位置（体基準）

	// 調整用パラメータ
	float headYawLimit_ = 85.0f * (std::numbers::pi_v<float> / 180.0f); // 顔の左右限界
	float headPitchLimit_ = 90.0f * (std::numbers::pi_v<float> / 180.0f); // 顔の上下限界
	float bodyFollowThresh_ = 90.0f * (std::numbers::pi_v<float> / 180.0f); // 追従を始める閾値
	float bodyTurnSpeedDeg_ = 300.0f; // 体の回頭速度(度/秒) …好みで 240〜360

	bool isDebugCamera_ = false;	// デバッグカメラフラグ

	uint32_t rightArmIndex_ = 2;	// 右腕部位のインデックス

	bool  isGrounded_ = true;		// 接地フラグ
	float groundY_ = 0.0f;			// 立っている床のY（暫定: 水平床）
	float vY_ = 0.0f;				// 縦速度 (m/s想定)
	float gravity_ = -30.75f;		// 重力加速度
	float jumpSpeed_ = 12.0f;		// 初速
	float maxFallSpeed_ = -50.0f;   // 最大落下速度（クランプ）

	WeaponConfig currentWeapon_;	// 現在装備中の武器設定（ランタイム用コピー）

	bool shotScheduled_ = false;    // クールダウン終了時に撃つ予約

	float fireCooldown_ = 0.0f;		// 次の射撃までの時間
	float fireInterval_ = 0.1f;		// 連射間隔（秒）→ 例: 600rpm ≒ 0.1s

	float recoilZ_ = 0.0f;          // 後退量（m相当のスケールでOK）
	float recoilPitch_ = 0.0f;      // 上向き回転（rad）
	float recoilYaw_ = 0.0f;        // 横ブレ（rad）

	float recoilVz_ = 0.0f;         // 速度
	float recoilVp_ = 0.0f;
	float recoilVy_ = 0.0f;

	float recoilReturn_ = 18.0f;    // ばね定数（戻りの強さ）
	float recoilDamping_ = 12.0f;   // 減衰
};

