#pragma once
#include "BaseCharacter.h"
#include <optional>

/// ---------- 前方宣言 ---------- ///
class Object3DCommon;


/// -------------------------------------------------------------
///							プレイヤークラス
/// -------------------------------------------------------------
class Player : public BaseCharacter
{
private: /// ---------- Behaviorの定義 ---------- ///

	// 振る舞い
	enum class Behavior
	{
		kRoot,	 // 通常状態
		kAttack, // 攻撃中
		kDash,	 // ダッシュ中
		kJump,	 // ジャンプ中
	};

	// 攻撃のフェーズ管理
	enum class AttackPhase
	{
		kRaise, // 振り上げ
		kSwing, // 振り下ろし
		kEnd    // 攻撃終了
	};

private: /// ---------- 構造体 ---------- ///

	// ダッシュ用ワーク
	struct WorkDash
	{
		// ダッシュ用の媒介変数
		uint32_t dashParameter_ = 0;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize(Object3DCommon* object3DCommon, Camera* camera) override;

	// 更新処理
	void Update() override;

	// 描画処理
	void Draw() override;

	// 中心座標を取得
	Vector3 GetCenterPosition() const override;

private: /// ---------- メンバ関数 ---------- ///

	// 調整項目の適用
	void ApplyGlobalVariables();

	// 浮遊ギミック初期化処理
	void InitializeFlaotingGimmick();

	// 浮遊ギミックの更新処理
	void UpdateFloatingGimmick();

	// 通常行動の初期化
	void BehaviorRootInitialize();

	// 通常行動の更新処理
	void BehaviorRootUpdate();

	// 攻撃行動の初期化処理
	void BehaviorAttackInitialize();

	// 攻撃行動の更新処理
	void BehaviorAttackUpdate();

	// ダッシュ行動初期化
	void BehaviorDashInitialize();

	// ダッシュ行動更新処理
	void BehaviorDashUpdate();

	// ジャンプ行動の初期化処理
	void BehaviorJumpInitialize();

	// ジャンプ行動の更新処理
	void BehaviorJumpUpdate();

private: /// ---------- メンバ変数 ---------- ///

	// 振る舞い
	Behavior behavior_ = Behavior::kRoot; // 通常状態

	AttackPhase attackPhase_ = AttackPhase::kRaise;
	float attackTimer_ = 0.0f;

	// 次の振る舞いをリクエスト
	std::optional<Behavior> behaviorRequest_ = std::nullopt;

	WorkDash workDash_;

	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 回転角度 (x, y, z)
	Vector3 velocity = { 0.0f, 0.0f, 0.0f }; // カメラの速度
	Vector3 targetVelocity_{};

	const float damping = 0.9f;            // 減衰率（小さいほど追従がゆっくり）
	const float stiffness = 0.1f;          // カメラの目標位置への「引っ張り力」

	// ギミックのパラメータ
	float floatingParameter_ = 0.0f;

	// ステップを決める
	const uint16_t periodTime = 60;  // 1.0f / 60.0f ではなく、フレーム数として定義
	const float step = 2.0f * PI / periodTime;

	// 腕の振り用のパラメータ
	float armSwingParameter_ = 0.0f;
	const float armSwingSpeed_ = 1.0f;  // 揺れの速さ
	const float armSwingAmplitude_ = 0.6f;  // 揺れの振幅

	bool isWeaponVisible_ = false; // 武器の表示フラグ
};
