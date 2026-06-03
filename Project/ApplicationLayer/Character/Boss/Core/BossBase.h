#pragma once
#include "BaseCharacter.h"
#include "BossTypes.h"

#include <Vector3.h>

#include "BossBrain.h"				// ボスの思考を担当するクラス
#include "BossAnimationComponent.h" // アニメーションを管理するクラス
#include "BossAttackComponent.h"	// 攻撃を管理するクラス
#include "BossMovementComponent.h"	// 移動を管理するクラス
#include "BossPhaseComponent.h"		// フェーズ移行を管理するクラス
#include "BossWeakPointComponent.h"	// 弱点を管理するクラス
#include "BossStatusComponent.h"	// HP やステータスを管理するクラス
#include "BossStateMachine.h"		// 状態遷移を管理するクラス
#include "IBossAttack.h"			// 攻撃のインターフェース

#include <memory>
#include <vector>

namespace K4E = Ken4lowEngine;

/// -----------------------------------------------------------
/// デバッグ用の簡易ヒット部位
/// 今後、正式な弱点システムへ差し替える前の確認用
/// -----------------------------------------------------------
enum class BossHitPart
{
	None,       // ヒットなし
	Head,       // 頭
	Body,       // 胴体
	LeftArm,    // 左腕
	RightArm,   // 右腕
	LeftLeg,    // 左脚
	RightLeg    // 右脚
};

/// -----------------------------------------------------------
/// 簡易ヒット判定の結果
/// DebugScene から返してもらう想定
/// -----------------------------------------------------------
struct BossHitResult
{
	bool isHit = false;					  // 何かしら当たったか
	BossHitPart part = BossHitPart::None; // どこに当たったか
	K4E::Vector3 hitPosition{};           // 当たったとみなした位置
	float damageMultiplier = 1.0f;        // 部位倍率（頭なら 2.0f など）
};

/// -----------------------------------------------------------
///					 ボス共通の基底クラス
/// -----------------------------------------------------------
class Player;

class BossBase : public BaseCharacter
{
public: /// ---------- ライフサイクル ---------- ///

	// デストラクタは仮想関数にしておく
	virtual ~BossBase() = default;

	/// <summary>
	/// 初期化
	/// BaseCharacter の共通構造を利用しつつ、ボス用部位を構築する
	/// </summary>
	virtual void Initialize() override;

	/// <summary>
	/// 更新
	/// 共通の更新順を制御する
	/// </summary>
	virtual void Update(float deltaTime) override;

	/// <summary>
	/// 描画
	/// BaseCharacter の部位描画 + 攻撃描画など
	/// </summary>
	virtual void Draw() override;

	/// <summary>
	/// シャドウ描画
	/// </summary>
	virtual void DrawShadow() override;

	/// <summary>
	/// デバッグ描画 / ImGui
	/// </summary>
	virtual void DrawImGui() override;

	/// <summary>
	/// 終了処理
	/// </summary>
	virtual void Finalize();

public: /// ---------- 衝突 ---------- ///

	/// <summary>
	/// ボス基底では純粋仮想にしておく
	/// 個別ボスでプレイヤー弾や近接ヒットなどを処理する
	/// </summary>
	virtual void OnCollision(K4E::Collider* other) override = 0;

public: /// ---------- ダメージ / 死亡 ---------- ///

	/// <summary>
	/// ダメージを受けたとき
	/// </summary>
	virtual void OnDamaged(float damage);

	/// <summary>
	/// プレイヤー銃弾でダメージを受けたとき
	/// </summary>
	virtual void OnBulletDamaged(float damage);

	/// <summary>
	/// 死亡時
	/// </summary>
	virtual void OnDead();

	// 生存中か
	bool IsAlive() const;

	// 死亡済みか
	bool IsDead() const;

public: /// ---------- 状態 / フェーズ管理 ---------- ///

	// 状態を取得
	BossState GetState() const { return state_; }

	// 状態を設定
	void SetState(BossState state) { state_ = state; }

	// フェーズを取得
	BossPhase GetPhase() const { return phase_; }

	// フェーズを設定
	void SetPhase(BossPhase phase) { phase_ = phase; }

public: /// ---------- 位置 / 向き ---------- ///

	/// <summary>
	/// 位置は body_.transform.translate_ を使う
	/// </summary>
	const K4E::Vector3& GetPosition() const { return body_.transform.translate_; }

	/// <summary>
	/// ボス本体の位置を設定する
	/// </summary>
	void SetPosition(const K4E::Vector3& position) { body_.transform.translate_ = position; }

	/// <summary>
	/// Yaw は body_.transform.rotate_.y を使う
	/// </summary>
	float GetYaw() const { return body_.transform.rotate_.y; }

	/// <summary>
	/// ボス本体のY回転を設定する
	/// </summary>
	void SetYaw(float yaw) { body_.transform.rotate_.y = yaw; }

	/// <summary>
	/// 中心座標は BaseCharacter の実装をそのまま使う
	/// 必要なら派生側で override 可
	/// </summary>
	virtual K4E::Vector3 GetCenterPosition() const override { return BaseCharacter::GetCenterPosition(); }

public: /// ---------- HP / ステータス ---------- ///

	// HP を取得
	float GetHP() const;

	// 最大HP を取得
	float GetMaxHP() const;

	// HP率を取得
	float GetHPRate() const;

public: /// ---------- ターゲット情報 ---------- ///

	// ターゲットの位置
	const K4E::Vector3& GetTargetPosition() const { return targetPosition_; }

	// ターゲットの位置をセット
	void SetTargetPosition(const K4E::Vector3& targetPosition) { targetPosition_ = targetPosition; }

	// ボス攻撃が実際にプレイヤーHPへ届くよう、ターゲットPlayer本体も保持する。
	void SetTargetPlayer(Player* player) { targetPlayer_ = player; }
	Player* GetTargetPlayer() const { return targetPlayer_; }
	bool ApplyDamageToTargetPlayer(float damage, const K4E::Vector3* attackPosition = nullptr);

public: /// ---------- 攻撃距離 / 判定補助 ---------- ///

	/// <summary>
	/// 攻撃可能距離を取得
	/// </summary>
	float GetAttackRange() const { return attackRange_; }

	/// <summary>
	/// 攻撃可能距離を設定
	/// </summary>
	void SetAttackRange(float attackRange) { attackRange_ = attackRange; }

	/// <summary>
	/// 現在クールタイム中か
	/// </summary>
	bool IsAttackCoolingDown() const { return attackCooldownTimer_ > 0.0f; }

	/// <summary>
	/// クールタイム残り時間を取得
	/// </summary>
	float GetAttackCooldownTimer() const { return attackCooldownTimer_; }

	/// <summary>
	/// 攻撃クールタイムを設定
	/// </summary>
	void SetAttackCooldown(float timeSec) { attackCooldownSec_ = timeSec; }

	/// <summary>
	/// ターゲットまでのXZ距離を返す
	/// </summary>
	float GetDistanceToTargetXZ() const;

	/// <summary>
	/// ターゲットが攻撃範囲内か
	/// </summary>
	bool IsTargetInAttackRange() const;

public: /// ---------- コンポーネント参照 ---------- ///

	/// <summary>
	/// 思考コンポーネントを取得
	/// </summary>
	BossBrain* GetBrain() { return brain_.get(); }
	const BossBrain* GetBrain() const { return brain_.get(); }

	/// <summary>
	/// 状態遷移コンポーネントを取得
	/// </summary>
	BossStateMachine* GetStateMachine() { return stateMachine_.get(); }
	const BossStateMachine* GetStateMachine() const { return stateMachine_.get(); }

	/// <summary>
	/// 移動コンポーネントを取得
	/// </summary>
	BossMovementComponent* GetMovementComponent() { return movementComponent_.get(); }
	const BossMovementComponent* GetMovementComponent() const { return movementComponent_.get(); }

	/// <summary>
	/// ステータスコンポーネントを取得
	/// </summary>
	BossStatusComponent* GetStatusComponent() { return statusComponent_.get(); }
	const BossStatusComponent* GetStatusComponent() const { return statusComponent_.get(); }

	/// <summary>
	/// 攻撃コンポーネントを取得
	/// </summary>
	BossAttackComponent* GetAttackComponent() { return attackComponent_.get(); }
	const BossAttackComponent* GetAttackComponent() const { return attackComponent_.get(); }

	/// <summary>
	/// アニメーションコンポーネントを取得
	/// </summary>
	BossAnimationComponent* GetAnimationComponent() { return animationComponent_.get(); }
	const BossAnimationComponent* GetAnimationComponent() const { return animationComponent_.get(); }

public: /// ---------- 攻撃登録 ---------- ///

	/// <summary>
	/// 攻撃を登録する
	/// 実体は BossAttackComponent に持たせる
	/// </summary>
	void RegisterAttack(std::unique_ptr<IBossAttack> attack);

protected: /// ---------- ダメージ通知 ---------- ///

	virtual void OnTargetPlayerDamaged(float damage);

public: /// ---------- 腕周り補助 ---------- ///

	/// <summary>
	/// 左腕のローカル回転を設定
	/// </summary>
	void SetLeftArmLocalRotate(const K4E::Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().leftArm;
		if (idx < parts.size())
		{
			parts[idx].transform.rotate_ = rotate;
		}
	}

	/// <summary>
	/// 右腕のローカル回転を設定
	/// </summary>
	void SetRightArmLocalRotate(const K4E::Vector3& rotate)
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().rightArm;
		if (idx < parts.size())
		{
			parts[idx].transform.rotate_ = rotate;
		}
	}

	/// <summary>
	/// 左腕根本のワールド座標を取得
	/// 攻撃エフェクトの発生位置に使いやすい
	/// </summary>
	K4E::Vector3 GetLeftArmRootWorldPosition()
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().leftArm;
		if (idx >= parts.size()) return GetCenterPosition();

		auto& body = GetBody();
		body.transform.Update();

		auto& arm = parts[idx];
		arm.transform.worldRotate_ = body.transform.worldRotate_;
		arm.transform.Update();

		return arm.transform.worldTranslate_;
	}

	/// <summary>
	/// 右腕根本のワールド座標を取得
	/// </summary>
	K4E::Vector3 GetRightArmRootWorldPosition()
	{
		auto& parts = GetBodyParts();
		const auto idx = GetPartIndices().rightArm;
		if (idx >= parts.size())
		{
			return GetCenterPosition();
		}

		auto& body = GetBody();
		body.transform.Update();

		auto& arm = parts[idx];
		arm.transform.worldRotate_ = body.transform.worldRotate_;
		arm.transform.Update();

		return arm.transform.worldTranslate_;
	}

public: /// ---------- デバッグ用 ---------- ///

	/// -----------------------------------------------------------
	/// デバッグ用の簡易球判定
	/// attackCenter / attackRadius を渡して、どの部位に当たったかを返す
	/// まずは DebugScene での仮ヒット確認用
	/// -----------------------------------------------------------
	BossHitResult CheckDebugHitSphere(const K4E::Vector3& attackCenter, float attackRadius);

	/// -----------------------------------------------------------
	/// ヒット結果を元にダメージ適用
	/// baseDamage * damageMultiplier で最終ダメージを決める
	/// -----------------------------------------------------------
	void ApplyDebugHitResult(const BossHitResult& hitResult, float baseDamage);

protected: /// ---------- 共通処理 ---------- ///

	/// -----------------------------------------------------------
	/// 各部位のワールド座標を返す
	/// 今後、弱点管理や正式な当たり判定にも流用しやすい
	/// -----------------------------------------------------------
	K4E::Vector3 GetPartWorldPosition(size_t partIndex);

	/// -----------------------------------------------------------
	/// 指定位置と半径で球ヒット判定
	/// attackCenter と partCenter の距離で簡易判定する
	/// -----------------------------------------------------------
	bool IsSphereHit(const K4E::Vector3& attackCenter, float attackRadius, const K4E::Vector3& targetCenter, float targetRadius) const;

public: /// ---------- 派生クラスが決めるもの ---------- ///

	/// <summary>
	/// ボス用の体幹・部位を構築する
	/// BaseCharacter::Initialize() は通常キャラ固定なので、
	/// ボスではこちらを使う
	/// </summary>
	virtual void BuildBossParts() = 0;

	/// <summary>
	/// 攻撃登録
	/// </summary>
	virtual void SetupAttacks() = 0;

	/// <summary>
	/// フェーズ設定
	/// </summary>
	virtual void SetupPhaseData() = 0;

	/// <summary>
	/// 弱点設定
	/// </summary>
	virtual void SetupWeakPoints() = 0;

	/// <summary>
	/// ボス固有初期化
	/// </summary>
	virtual void SetupBoss() = 0;

protected: /// ---------- 共通更新の中で分割する処理 ---------- ///

	// 状態更新
	virtual void UpdateState(float deltaTime);

	// フェーズ更新
	virtual void UpdatePhase(float deltaTime);

	// 移動更新
	virtual void UpdateMovement(float deltaTime);

	// 攻撃更新
	virtual void UpdateAttack(float deltaTime);

	// 弱点更新
	virtual void UpdateWeakPoints(float deltaTime);

	// 死亡チェック
	virtual void CheckDeath();

protected: /// ---------- 共通情報群 ---------- ///

	// 追跡対象
	K4E::Vector3 targetPosition_{};
	Player* targetPlayer_ = nullptr;

	// 状態
	BossState state_ = BossState::Intro;
	BossPhase phase_ = BossPhase::Phase1;

protected: /// ---------- 攻撃まわり共通値 ---------- ///

	// 攻撃可能距離
	float attackRange_ = 3.0f;

	// 攻撃終了後の再攻撃待ち
	float attackCooldownSec_ = 1.0f;
	float attackCooldownTimer_ = 0.0f;

protected: /// ---------- コンポーネント群 ---------- ///

	// 思考
	std::unique_ptr<BossBrain> brain_;

	// 状態遷移
	std::unique_ptr<BossStateMachine> stateMachine_;

	// 攻撃
	std::unique_ptr<BossAttackComponent> attackComponent_;

	// 移動
	std::unique_ptr<BossMovementComponent> movementComponent_;

	// フェーズ移行
	//std::unique_ptr<BossPhaseComponent> phaseComponent_;

	// 弱点
	//std::unique_ptr<BossWeakPointComponent> weakPointComponent_;

	// HP やステータス
	std::unique_ptr<BossStatusComponent> statusComponent_;

	// アニメーション
	std::unique_ptr<BossAnimationComponent> animationComponent_;
};