#pragma once
#include "BaseCharacter.h"
#include <Object3D.h>
#include <BossConfig.h>
#include "IBossAttack.h"

#include <memory>

/// -------------------------------------------------------------
///					　		ボスクラス
/// -------------------------------------------------------------
class Boss : public BaseCharacter
{
public: /// ---------- 構造体 ---------- ///

	// ツタ薙ぎ払い攻撃フェーズ列挙型
	enum class VinePhase { Idle, Windup, Active, Recovery, Cooldown };

	// ツタ薙ぎ払い攻撃フェーズ
	struct VineSweepRuntime
	{
		VinePhase phase = VinePhase::Idle;
		float phaseTimer = 0.0f;     // 今フェーズ内経過
		float cooldownTimer = 0.0f;  // クールダウン残り
		float lockedYaw = 0.0f;      // 攻撃開始時の向き（Yaw）
		bool  didHit = false;        // 1回だけ当てる用
		bool  visible = false;       // vineObject_ を描くか
	} vine_;

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	virtual ~Boss() = default;

	// 初期化処理
	virtual void Initialize() override;

	// 更新処理
	virtual void Update(float deltaTime) override;

	// 描画処理
	virtual void Draw() override;

	// ImGui描画処理
	virtual void DrawImGui() override;

	// 衝突判定処理
	virtual void OnCollision(Collider* other) override;

	// 中心座標を取得
	virtual Vector3 GetCenterPosition() const override;

public: /// ---------- 腕周りの関数 ---------- ///

	// 左腕部位のローカル回転を設定
	void SetLeftArmLocalRotate(const Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().leftArm; // 左腕部位インデックス
		if (idx < parts.size()) {
			parts[idx].transform.rotate_ = rotate; // ローカル回転
		}
	}

	// 左腕部位のワールド座標を取得
	Vector3 GetLeftArmRootWorldPosition()
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().leftArm;
		if (idx >= parts.size()) return GetCenterPosition();

		// BaseCharacter::UpdateHierarchy と同じ計算に寄せる
		auto& body = GetBody();
		body.transform.Update();

		auto& arm = parts[idx];
		arm.transform.worldRotate_ = body.transform.worldRotate_;
		arm.transform.Update();

		return arm.transform.worldTranslate_;
	}

	// 右腕部位のローカル回転を設定
	void SetRightArmLocalRotate(const Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().rightArm; // 右腕部位インデックス
		if (idx < parts.size()) {
			parts[idx].transform.rotate_ = rotate; // ローカル回転
		}
	}

	// 右腕部位のワールド座標を取得
	Vector3 GetRightArmRootWorldPosition()
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().rightArm;
		if (idx >= parts.size()) return GetCenterPosition();

		// BaseCharacter::UpdateHierarchy と同じ計算に寄せる
		auto& body = GetBody();
		body.transform.Update();

		auto& arm = parts[idx];
		arm.transform.worldRotate_ = body.transform.worldRotate_;
		arm.transform.Update();
		return arm.transform.worldTranslate_;
	}

public: /// ---------- アクセッサ ---------- ///

	ForestBossParams& GetParams() { return params_; }

	const ForestBossParams& GetParams() const { return params_; }

private: /// ---------- メンバ変数 ---------- ///

	ForestBossParams params_{}; // ひとまずデフォルト（後でJSON読み込み）

	std::unique_ptr<IBossAttack> vineSweepAttack_;  // ツタ薙ぎ払い攻撃
	std::unique_ptr<IBossAttack> seedMortarAttack_; // 種子迫撃攻撃

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

