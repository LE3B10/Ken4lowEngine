#pragma once
#include "Core/BossBase.h"

/// -------------------------------------------------------------
/// 人型ボス共通基底
///
/// 役割:
/// - 頭、胴体、左右腕、左右脚の構築
/// - 人型ボス共通の初期スケール / ローカル配置
/// - 共通の向き制御
///
/// 方針:
/// - 状態更新 / 移動更新 / 攻撃更新 / アニメ更新は持たない
/// - それらは BossBase + 各Component + 派生クラスへ任せる
/// - このクラスは「人型ボスの見た目の土台」に徹する
/// -------------------------------------------------------------
class HumanoidBossBase : public BossBase
{
public:
	virtual ~HumanoidBossBase() = default;

public: /// ---------- BossBase override ---------- ///

	/// <summary>
	/// 人型部位を構築する
	/// </summary>
	void BuildBossParts() override;

	/// <summary>
	/// 攻撃設定
	/// 基底では特別な攻撃登録はしない
	/// </summary>
	void SetupAttacks() override {}

	/// <summary>
	/// フェーズ設定
	/// 基底では何もしない
	/// </summary>
	void SetupPhaseData() override {}

	/// <summary>
	/// 弱点設定
	/// 基底では何もしない
	/// </summary>
	void SetupWeakPoints() override {}

	/// <summary>
	/// 共通初期設定
	/// 人型ボスとして最低限の初期化だけ行う
	/// 実際の行動開始状態などは派生側で上書きしてよい
	/// </summary>
	void SetupBoss() override;

	/// <summary>
	/// 衝突処理
	/// 現段階では簡易空実装
	/// </summary>
	void OnCollision(K4E::Collider* other) override;

	/// <summary>
	/// デバッグ表示
	/// </summary>
	void DrawImGui() override;

protected: /// ---------- BossBase 共通更新差し込み ---------- ///

	/// <summary>
	/// 人型基底では状態更新を持たない
	/// 派生ボス側で管理する
	/// </summary>
	void UpdateState(float deltaTime) override;

	/// <summary>
	/// 人型基底では移動更新を持たない
	/// 派生ボスまたは MovementComponent 側で管理する
	/// </summary>
	void UpdateMovement(float deltaTime) override;

	/// <summary>
	/// 人型基底では攻撃更新を持たない
	/// 派生ボスまたは AttackComponent 側で管理する
	/// </summary>
	void UpdateAttack(float deltaTime) override;

	/// <summary>
	/// 死亡チェック
	/// 基底では BossBase 側の共通死亡処理を使う
	/// </summary>
	void CheckDeath() override;

protected: /// ---------- 共通補助 ---------- ///

	/// <summary>
	/// プレイヤー方向へゆっくり向く
	/// 人型ボス共通の補助関数として残す
	/// </summary>
	void FaceTarget(float deltaTime, float rotateSpeed);

	/// <summary>
	/// ターゲットまでXZ平面距離を返す
	/// </summary>
	float GetDistanceToTargetXZ() const;

protected: /// ---------- 派生で調整しやすい数値 ---------- ///

	virtual float GetMoveSpeed() const { return 2.2f; }
	virtual float GetRotateSpeed() const { return 4.0f; }

	virtual float GetIdleToMoveDistance() const { return 6.0f; }
	virtual float GetAttackRange() const { return 2.8f; }

	virtual float GetAttackDuration() const { return 0.75f; }
	virtual float GetAttackCooldown() const { return 1.2f; }

	virtual float GetStaggerTime() const { return 0.25f; }

protected: /// ---------- モデルパス取得 ---------- ///

	virtual std::string GetBodyModelPath() const { return "Characters/body.gltf"; }
	virtual std::string GetHeadModelPath() const { return "Characters/head.gltf"; }
	virtual std::string GetLeftArmModelPath() const { return "Characters/left_arm.gltf"; }
	virtual std::string GetRightArmModelPath() const { return "Characters/right_arm.gltf"; }
	virtual std::string GetLeftLegModelPath() const { return "Characters/left_leg.gltf"; }
	virtual std::string GetRightLegModelPath() const { return "Characters/right_leg.gltf"; }

protected: /// ---------- 初期配置取得 ---------- ///

	virtual K4E::Vector3 GetInitialBodyPosition() const { return { 0.0f, 2.25f, 0.0f }; }
	virtual K4E::Vector3 GetInitialBodyScale() const { return { 1.0f, 1.0f, 1.0f }; }

	virtual K4E::Vector3 GetHeadLocalOffset() const { return { 0.0f,  0.75f, 0.0f }; }
	virtual K4E::Vector3 GetLeftArmLocalOffset() const { return { -0.75f, 0.75f, 0.0f }; }
	virtual K4E::Vector3 GetRightArmLocalOffset() const { return { 0.75f, 0.75f, 0.0f }; }
	virtual K4E::Vector3 GetLeftLegLocalOffset() const { return { -0.25f,-0.75f, 0.0f }; }
	virtual K4E::Vector3 GetRightLegLocalOffset() const { return { 0.25f,-0.75f, 0.0f }; }

	virtual K4E::Vector3 GetHeadScale() const { return { 1.0f, 1.0f, 1.0f }; }
	virtual K4E::Vector3 GetArmScale() const { return { 1.0f, 1.0f, 1.0f }; }
	virtual K4E::Vector3 GetLegScale() const { return { 1.0f, 1.0f, 1.0f }; }
};