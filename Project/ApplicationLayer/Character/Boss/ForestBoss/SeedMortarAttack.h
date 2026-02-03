#pragma once
#include "IBossAttack.h"
#include "BossConfig.h"
#include <Object3D.h>

#include <vector>
#include <memory>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
class Boss;


/// -------------------------------------------------------------
///					　種子ミサイル攻撃クラス
/// -------------------------------------------------------------
class SeedMortarAttack : public IBossAttack
{
private: /// ---------- 列挙型 ---------- ///

	// 種子攻撃フェーズ列挙型
	enum class Phase
	{
		Idle,	  // 待機
		Windup,	  // 溜め
		Active,	  // 有効
		Recovery, // 回復
		Cooldown, // クールダウン
	};

private: /// ---------- 構造体 ---------- ///

	/// ---------- 1個の種のランタイム ---------- ///
	struct SeedInstance
	{
		bool    active = false;  // まだ上昇中か
		bool    exploded = false;  // 爆発済みか（演出用）
		K4E::Vector3 groundPos{};       // 地面上の中心位置
		K4E::Vector3 position{};        // 現在位置（描画用）
		float   timer = 0.0f;   // 出現してからの経過時間

		std::unique_ptr<K4E::Object3D> object;
	};

	/// ---------- 攻撃全体のランタイム ---------- ///
	struct SeedMortarRuntime
	{
		Phase phase = Phase::Idle;
		float phaseTimer = 0.0f;  // 今フェーズ内経過
		float cooldownTime = 0.0f;  // クールダウン残り
		float lockedYaw = 0.0f;  // 攻撃開始時の向き
		bool  visible = false; // 種を描画するか
		bool  didHit = false; // ダメージが当たったか（後で使用）
	} runtime_;

	// ボスの腕ポーズ用構造体
	struct PoseParams
	{
		float raiseX = -1.4f;	// 腕を上げる角度
		float openYawMin = 0.8f;	// 溜め中に腕を開く角度
		float openYawMax = -0.4f; // 腕を最大まで開く角度
		float clapX = -1.4f;	// 種を放つときに腕を閉じる角度
		float holdUntil = 0.8f; // Active 中、どこまで溜めを維持するか
	} poseParams_;

public: /// ---------- メンバ関数 ---------- ///

	// 攻撃名を取得
	const char* GetName() const override { return "Seed Mortar Attack"; }

	// 初期化処理
	void Initialize() override;

	// クールダウン処理
	void TickCooldown(float deltaTime) override;

	// 攻撃可能か
	bool CanAttack() const override;

	// 攻撃実行
	void Attack() override;

	// 更新処理
	void Update(Boss* boss, float deltaTime, float bossYawRad, const K4E::Vector3& playerPosition) override;

	// 攻撃がアクティブか
	bool IsActive() const override;

	// 描画処理
	void Draw() override;

	// ImGui描画処理
#ifdef USE_IMGUI
	void DrawImGui(Boss& boss) override;
#endif // USE_IMGUI

private: /// ---------- フェーズの更新処理関数 ---------- ///

	// Idleフェーズ更新
	void UpdatePhase_Idle(float bossYawRad);

	// Windupフェーズ更新
	void UpdatePhase_Windup(Boss* boss, float deltaTime);

	// Activeフェーズ更新
	void UpdatePhase_Active(Boss* boss, float deltaTime, const K4E::Vector3& playerPosition);

	// Recoveryフェーズ更新
	void UpdatePhase_Recovery(Boss* boss, float deltaTime);

	// ボスのポーズ更新
	void UpdateBossPose(Boss* boss);

	// 種をまとめて出現させる
	void SpawnSeeds(const K4E::Vector3& centerXZ, const K4E::Vector3& bossCenterXZ, const ForestBossParams::SeedMortar& params);

private: /// ---------- メンバ変数 ---------- ///

	// 同時に出ている種たち
	std::vector<SeedInstance> seeds_;

	bool requestStart_ = false; // 攻撃開始要求フラグ

	bool spawned_ = false; // Active で種をスポーン済みか

private: /// ---------- デバッグ用メンバ変数 ---------- ///

#ifdef USE_IMGUI
	// ヒット関連（後でダメージを付けるとき用）
	int   debugHitCount_ = 0;
	bool  debugLastHit_ = false;
	float debugLastHitDist_ = 0.0f;

	// テスト用プレイヤー位置
	bool    debugUseTestPlayerPos_ = false;
	K4E::Vector3 debugTestPlayerPos_{ 0.0f, 0.0f, 0.0f };

	// 腕のポーズ調整用
	bool debugEditPose_ = false;
	K4E::Vector3 debugLeftArmRotate_{ 0.0f, 0.0f, 0.0f };
	K4E::Vector3 debugRightArmRotate_{ 0.0f, 0.0f, 0.0f };
#endif // USE_IMGUI
};

