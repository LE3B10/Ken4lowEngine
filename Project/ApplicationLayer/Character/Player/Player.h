#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include <FpsCamera.h>
#include "FireState.h"
#include "DeathState.h"

#include "WeaponManager.h"

#include <memory>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class Input;


/// -------------------------------------------------------------
///					　プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
private: /// ---------- 構造体 ---------- ///

	// ビュー状態構造体
	struct ViewState
	{
		// 体のYaw（ラジアン）
		float bodyYaw = 0.0f;

		// 頭のローカルYaw（親=体に対する差）
		float headYawLocal = 0.0f;

		// 頭のYaw制限（ラジアン）: 85度
		float headYawLimit = 85.0f * (std::numbers::pi_v<float> / 180.0f);

		// 頭のピッチ制限（ラジアン） : 90度
		float headPitchLimit = 90.0f * (std::numbers::pi_v<float> / 180.0f);;

		// 体が頭を追従し始める閾値（ラジアン） : 90度
		float bodyFollowThresh = 90.0f * (std::numbers::pi_v<float> / 180.0f);;

		// 体の回頭速度(度/秒)
		float bodyTurnSpeedDeg = 300.0f;

		// デバッグカメラフラグ
		bool isDebugCamera = false;
	};

	// 移動状態構造体
	struct MovementState
	{
		bool isGrounded = true;		 // 接地フラグ
		float groundY = 0.0f;		 // 立っている床のY（暫定: 水平床）
		float vY = 0.0f;			 // 縦速度
		float gravity = -30.75f;	 // 重力加速度
		float jumpSpeed = 12.0f;	 // ジャンプ初速
		float maxFallSpeed = -50.0f; // 最大落下速度（クランプ）
	};

	// リコイル構造体
	struct RecoilState
	{
		float z = 0.0f;          // 後退量（m相当のスケールでOK）
		float pitch = 0.0f;      // 上向き回転（rad）
		float yaw = 0.0f;        // 横ブレ（rad）
		float vz = 0.0f;         // 速度
		float vp = 0.0f;		 // 速度
		float vy = 0.0f;	     // 速度
		float kReturn = 18.0f;   // ばね定数（戻りの強さ）
		float dumping = 12.0f;   // 減衰
	};

	// ディゾルブエフェクト構造体
	struct DissolveEffect
	{
		float threshold = 1.0f;   // 閾値
		float edgeThickness = 0.16f; // 縁の太さ
		Vector4 edgeColor = { 0.2f,0.8f,1.0f,1.0f }; // 縁色
	};

	// 各部位のインデックス
	struct PartIndices
	{
		const uint32_t head = 0;	 // 頭
		const uint32_t leftArm = 1;  // 左腕
		const uint32_t rightArm = 2; // 右腕
		const uint32_t leftLeg = 3;	 // 左脚
		const uint32_t rightLeg = 4; // 右脚
	};

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
	void OnCollision(Collider* other) override;

	// 中心座標を取得する純粋仮想関数
	Vector3 GetCenterPosition() const override;

public: /// ---------- アクセサー関数 ---------- ///

	// デバッグカメラフラグ取得
	bool IsDebugCamera() const { return viewState_.isDebugCamera; }
	void SetDebugCamera(bool isDebug) { viewState_.isDebugCamera = isDebug; }

	// FPSカメラ取得
	FpsCamera* GetFpsCamera() const { return fpsCamera_.get(); }

	// プレイヤーモデル取得
	Object3D* GetPlayerModel() const { return body_.object.get(); }

private: /// ---------- メンバ関数 ---------- ///

	// 移動処理
	void Move(float deltaTime);

	// 武器選択
	void SelectWeapon(const std::string& name);

	// 死亡処理開始
	void StartDeath(DeathMode mode);

	// 死亡処理更新
	void UpdateDeath(float deltaTime);

private: /// ----------メンバ変数 ---------- ///

	Input* input_ = nullptr; // 入力クラス

	std::unique_ptr<FpsCamera> fpsCamera_; // FPSカメラ

	std::unique_ptr<WeaponManager> weaponManager_; // 武器マネージャー

	// ビュー状態構造体
	ViewState viewState_ = {};

	// 移動状態構造体
	MovementState movementState_ = {};

	// 射撃状態構造体
	FireState fireState_ = {};

	// リコイル状態構造体
	RecoilState recoilState_ = {};

	// 死亡状態構造体
	DeathState deathState_ = {};

	// ディゾルブエフェクト構造体
	DissolveEffect dissolveEffect_ = {};

	// 各部位のインデックス
	PartIndices partIndices_ = {};

private: /// ---------- 設定値 ---------- ///

	const std::string kWeaponDir = "Resources/JSON/weapons";		   // 武器データディレクトリ
	const std::string kWeaponMonolith = "Resources/JSON/weapons.json"; // 武器データモノリス
};

