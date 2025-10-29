#pragma once
#include <BaseCharacter.h>
#include <Object3D.h>
#include <FpsCamera.h>
#include "FireState.h"
#include "DeathState.h"
#include "ContactRecord.h"

#include "WeaponManager.h"

#include <memory>
#include <numbers>

/// ---------- 前方宣言 ---------- ///
class Input;
class LevelObjectManager;
class CollisionManager;
class Enemy;

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

	// ジャンプ状態構造体
	struct JumpState
	{
		bool isGrounded = false;		// 接地判定
		float gravity = 0.01f;			// 重力の値
		float jumpVelocity = 0.0f;		// ジャンプの上向き速度
		const float jumpPower = 0.25f;  // ジャンプ初速度
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

	// レベルオブジェクトマネージャー設定
	void SetLevelObjectManager(LevelObjectManager* mgr) { levelObjectManager_ = mgr; }

	// 衝突マネージャー設定
	void SetCollisionManager(CollisionManager* mgr) { collisionManager_ = mgr; }

	// 敵キャラクター設定
	void SetEnemyPointer(Enemy* enemy) { enemy_ = enemy; }

	// コライダー登録
	void RegisterColliders(CollisionManager* mgr);

	// 敵の攻撃などで吹っ飛ばす用
	void AddKnockback(const Vector3& impulse) { knockbackVel_ += impulse; }

	// 敵から殴られた瞬間のリアクション用
	// dir           : プレイヤーから見て押し返す方向（Enemy側でプレイヤー→敵の向きから計算してるやつ）
	// horizontalPow : 水平ノックバックの強さ
	// upPow         : 真上に跳ねる初速
	void ApplyDamageImpulse(const Vector3& dir, float horizontalPow, float upPow);

	// ダメージを受けたときの処理
	void TakeDamage(float amount);

	// 体力取得
	float GetHp() const { return hp_; }

	// 最大体力取得
	float GetMaxHp() const { return maxHp_; }

	// 死亡中かどうか取得
	bool IsDeadNow() const { return deathState_.isDead || deathState_.inDeathSeq; }

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
	LevelObjectManager* levelObjectManager_ = nullptr; // レベルオブジェクトマネージャー
	CollisionManager* collisionManager_ = nullptr; // 衝突マネージャー
	Enemy* enemy_ = nullptr; // 敵キャラクター

	ContactRecord contactRecord_; // 接触記録

	std::unique_ptr<FpsCamera> fpsCamera_; // FPSカメラ

	std::unique_ptr<WeaponManager> weaponManager_; // 武器マネージャー

	// ビュー状態構造体
	ViewState viewState_ = {};

	// ジャンプ状態構造体
	JumpState jumpState_ = {};

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

	// ノックバック速度（毎フレームMove()で加算＆減衰させる）
	Vector3 knockbackVel_ = { 0.0f, 0.0f, 0.0f };

	// 減衰係数。1フレームごとにこれを掛けるイメージ
	float knockbackDamping_ = 1.0f;

	// ノックバック関連のチューニング
	float hurtKnockbackGain_ = 0.4f;     // どれくらい素早く目標の押し戻し速度に近づくか(0〜1)
	float groundKnockbackDamping_ = 0.8f; // 地上のときの減衰（今までの0.8fそのまま）
	float airKnockbackDamping_ = 0.93f; // 空中のときの減衰（ゆっくり失速 = 後ろにスーッと流れる）

	float centerOffsetY_ = 2.0f;  // 足裏ピボットなら -half.y を入れる

	std::string skinTexturePath_ = "steve.png"; // スキンテクスチャパス

	// 体力
	float maxHp_ = 100.0f;
	float hp_ = 100.0f;

	// ヒット後の無敵（連続でゴリゴリ削られないように）
	float hurtInvincibleTime_ = 0.5f; // 0.5秒くらいヒット無敵
	float hurtTimer_ = 0.0f;
};

