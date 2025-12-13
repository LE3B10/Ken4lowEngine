#pragma once
#include "IBossAttack.h"
#include "BossConfig.h"
#include <Object3D.h>

#include <memory>

/// ---------- 前方宣言 ---------- ///
class Boss;

/// -------------------------------------------------------------
///					　ツタ薙ぎ払い攻撃クラス
/// -------------------------------------------------------------
class VineSweepAttack : public IBossAttack
{
private: /// ---------- 列挙型 ---------- ///

	// ツタ薙ぎ払い攻撃フェーズ列挙型
	enum class Phase
	{
		Idle,	  // 待機
		Windup,	  // 溜め
		Active,	  // 有効
		Recovery, // 回復
		Cooldown, // クールダウン
	};

private: /// ---------- 構造体 ---------- ///

	// ツタ薙ぎ払い攻撃フェーズ
	struct VineSweepRuntime
	{
		Phase phase = Phase::Idle;
		float phaseTimer_ = 0.0f;     // 今フェーズ内経過
		float cooldownTimer = 0.0f;  // クールダウン残り
		float lockedYaw = 0.0f;      // 攻撃開始時の向き（Yaw）
		bool  didHit = false;        // 1回だけ当てる用
		bool  visible = false;       // vineObject_ を描くか
	} vine_;

	// ツタの残像情報
	struct AfterImage
	{
		Vector3 pos{};
		float yaw = 0.0f;
		float reach = 1.0f;  // 伸び具合（Windup連動用に使ってもOK）
		float age = 0.0f;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 攻撃名を取得
	const char* GetName() const override { return "Vine Sweep Attack"; }

	// 初期化処理
	void Initialize() override;

	// クールダウン処理
	void TickCooldown(float deltaTime) override;

	// 攻撃可能か
	bool CanAttack() const override;

	// 攻撃実行
	void Attack() override;

	// 更新処理
	void Update(Boss* boss, float deltaTime, float bossYawRad, const Vector3& playerPosition) override;

	// 攻撃がアクティブか
	bool IsActive() const override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
#ifdef USE_IMGUI
	void DrawImGui(Boss& boss) override;
#endif // USE_IMGUI

private: /// ---------- メンバ関数 ---------- ///

	// XZ平面上でセクター内に点があるか判定する
	bool IsPointInSectorXZ(const Vector3& position, const Vector3& origin, float forwardYawRad, float radius, float angleDeg, float yThickness) const;

	// ツタのビジュアル更新
	void UpdateVineVisual(Boss* boss);

	// 左腕更新
	void LeftArmUpdate(Boss* boss);

	// ツタ残像生成
	void SpawnAfterImage();

	// ツタ残像更新
	void UpdateAfterImages(float deltaTime);

private: /// ---------- フェーズの更新処理関数 ---------- ///

	// Idleフェーズ更新
	void UpdatePhase_Idle(float bossYawRad);

	// Windupフェーズ更新
	void UpdatePhase_Windup(Boss* boss, float deltaTime);

	// Activeフェーズ更新
	void UpdatePhase_Active(Boss* boss, float deltaTime, const Vector3& playerPosition);

	// Recoveryフェーズ更新
	void UpdatePhase_Recovery(Boss* boss, float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Object3D> vineObject_; // つるオブジェクト

	bool requestStart_ = false; // 攻撃開始要求フラグ

private: /// ---------- ツタ残像用メンバ変数 ---------- ///

	bool trailEnabled_ = true;
	float trailSpawnInterval_ = 0.02f; // 0.015〜0.03がおすすめ
	float trailLife_ = 0.14f;          // 0.10〜0.18がおすすめ
	int   trailMax_ = 12;

	float trailTimer_ = 0.0f;
	std::vector<AfterImage> afterImages_;
	std::vector<std::unique_ptr<Object3D>> trailObjects_;

	Vector3 lastVinePos_{};
	float   lastVineYaw_ = 0.0f;
	float   lastVineReach_ = 1.0f;

private: /// ---------- デバッグ用メンバ変数 ---------- ///

#ifdef USE_IMGUI
	bool debugStartVineSweep_ = false;

	// テスト用プレイヤー座標（プレイヤー接続できてない間の確認用）
	bool  debugUseTestPlayerPos_ = true;
	Vector3 debugTestPlayerPos_{ 0.0f, 0.0f, 4.0f };

	// 状態の可視化
	bool  debugPlayerInSector_ = false;
	int   debugHitCount_ = 0;
	float debugHitFlash_ = 0.0f;   // HIT表示を出す秒数

	// 角度・補間の見える化
	float debugStartYaw_ = 0.0f;
	float debugEndYaw_ = 0.0f;
	float debugYaw_ = 0.0f;
	float debugT_ = 0.0f;
#endif
};

