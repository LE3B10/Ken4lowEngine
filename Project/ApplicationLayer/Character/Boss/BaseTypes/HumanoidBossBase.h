#pragma once
#include "Core/BossBase.h"

/// -------------------------------------------------------------
///						人型ボス共通基底
/// -------------------------------------------------------------
class HumanoidBossBase : public BossBase
{
public: /// ---------- 基本構造 ---------- ///

	// デストラクタは仮想関数にしておく
	virtual ~HumanoidBossBase() = default;

public: /// ---------- BossBase override ---------- ///

	/// <summary>
	/// 人型部位を構築する
	/// </summary>
	void BuildBossParts() override;

	/// <summary>
	/// 共通初期設定
	/// 人型ボスとして最低限の初期化だけ行う
	/// 実際の行動開始状態などは派生側で上書きしてよい
	/// </summary>
	void SetupBoss() override;

	/// <summary>
	/// デバッグ表示
	/// </summary>
	void DrawImGui() override;

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

	// 移動速度
	virtual float GetMoveSpeed() const { return 2.2f; }

	// 回転速度
	virtual float GetRotateSpeed() const { return 4.0f; }

	// プレイヤーを追いかける距離
	virtual float GetIdleToMoveDistance() const { return 6.0f; }

	// プレイヤーを追いかけるのをやめる距離
	virtual float GetAttackRange() const { return 2.8f; }

	// 攻撃の持続時間
	virtual float GetAttackDuration() const { return 0.75f; }

	// 攻撃後の硬直時間
	virtual float GetAttackCooldown() const { return 1.2f; }

	// ダメージを受けたときの硬直時間
	virtual float GetStaggerTime() const { return 0.25f; }

protected: /// ---------- モデルパス取得 ---------- ///

	// モデルパス
	virtual std::string GetBodyModelPath() const;
	virtual std::string GetHeadModelPath() const;
	virtual std::string GetLeftArmModelPath() const;
	virtual std::string GetRightArmModelPath() const;
	virtual std::string GetLeftLegModelPath() const;
	virtual std::string GetRightLegModelPath() const;

protected: /// ---------- 初期配置取得 ---------- ///

	// 初期位置やスケール

	// ボディの初期位置
	virtual K4E::Vector3 GetInitialBodyPosition() const { return { 0.0f, 2.25f, 0.0f }; }

	// ボディの初期スケール
	virtual K4E::Vector3 GetInitialBodyScale() const { return { 1.0f, 1.0f, 1.0f }; }

	// 各部位のローカルオフセット
	virtual K4E::Vector3 GetHeadLocalOffset() const { return { 0.0f,  0.75f, 0.0f }; }
	virtual K4E::Vector3 GetLeftArmLocalOffset() const { return { -0.75f, 0.75f, 0.0f }; }
	virtual K4E::Vector3 GetRightArmLocalOffset() const { return { 0.75f, 0.75f, 0.0f }; }
	virtual K4E::Vector3 GetLeftLegLocalOffset() const { return { -0.25f,-0.75f, 0.0f }; }
	virtual K4E::Vector3 GetRightLegLocalOffset() const { return { 0.25f,-0.75f, 0.0f }; }

	// 各部位のスケール
	virtual K4E::Vector3 GetHeadScale() const { return { 1.0f, 1.0f, 1.0f }; }
	virtual K4E::Vector3 GetArmScale() const { return { 1.0f, 1.0f, 1.0f }; }
	virtual K4E::Vector3 GetLegScale() const { return { 1.0f, 1.0f, 1.0f }; }
};
