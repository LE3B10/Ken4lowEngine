#include "BossBase.h"
#include "CollisionTypeIdDef.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossBase::Initialize()
{
	// コライダータイプの設定
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBoss));
	Collider::SetOBBHalfSize({ 1.0f, 1.0f, 1.0f }); // 仮。後でボスごとに差し替え

	// ボス用部位を構築
	BuildBossParts();

	// 思考コンポーネント生成
	brain_ = std::make_unique<BossBrain>();
	brain_->Initialize(this);

	// ステータス生成
	statusComponent_ = std::make_unique<BossStatusComponent>();
	statusComponent_->Initialize(300.0f); // 仮。後でボスごとに差し替え

	// 状態遷移生成
	stateMachine_ = std::make_unique<BossStateMachine>();
	stateMachine_->Initialize(state_); // 仮。後で開始状態を差し替え

	// 移動コンポーネント生成
	movementComponent_ = std::make_unique<BossMovementComponent>();
	movementComponent_->Initialize(2.0f, 4.0f, 3.0f); // 仮。後でボスごとに差し替え

	// ---------------------------------------------------------
	// アニメーションコンポーネント生成
	// 見た目の歩行 / 攻撃 / 待機を担当させる
	// ---------------------------------------------------------
	animationComponent_ = std::make_unique<BossAnimationComponent>();
	animationComponent_->Initialize(this);

	// 攻撃コンポーネント生成
	attackComponent_ = std::make_unique<BossAttackComponent>();

	// 攻撃距離・クールタイム初期値
	attackRange_ = 3.0f;
	attackCooldownSec_ = 1.2f;
	attackCooldownTimer_ = 0.0f;

	// 派生側設定
	SetupAttacks();
	SetupPhaseData();
	SetupWeakPoints();
	SetupBoss();

	// 攻撃初期化
	attackComponent_->Initialize(this);
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossBase::Update(float deltaTime)
{
	// 攻撃クールタイム更新
	if (attackCooldownTimer_ > 0.0f)
	{
		attackCooldownTimer_ -= deltaTime;
		if (attackCooldownTimer_ < 0.0f)
		{
			attackCooldownTimer_ = 0.0f;
		}
	}

	// ステータス更新
	if (statusComponent_)
	{
		statusComponent_->Update(deltaTime);
	}

	// ステートマシン更新
	if (stateMachine_)
	{
		stateMachine_->Update(*this, deltaTime);

		// BossBase 側の状態変数もステートマシンに合わせる
		state_ = stateMachine_->GetCurrentState();
	}

	// 状態ごとの更新
	UpdateState(deltaTime);
	UpdatePhase(deltaTime);
	UpdateMovement(deltaTime);
	UpdateAttack(deltaTime);

	// アニメーション更新
	if (animationComponent_)
	{
		animationComponent_->Update(*this, deltaTime);
	}

	UpdateWeakPoints(deltaTime);

	// 死亡確認
	CheckDeath();

	Collider::SetCenterPosition(GetBody().object->GetTranslate());

	// 最後に部位階層更新
	BaseCharacter::Update(deltaTime);
}

/// -------------------------------------------------------------
/// 描画
/// -------------------------------------------------------------
void BossBase::Draw()
{
	// 本体と部位描画
	BaseCharacter::Draw();

	// 攻撃描画
	if (attackComponent_)
	{
		attackComponent_->Draw();
	}
}

/// -------------------------------------------------------------
/// シャドウ描画
/// -------------------------------------------------------------
void BossBase::DrawShadow()
{
	BaseCharacter::DrawShadow();

	if (attackComponent_)
	{
		attackComponent_->DrawShadow();
	}
}

/// -------------------------------------------------------------
/// ImGui描画
/// -------------------------------------------------------------
void BossBase::DrawImGui()
{
#ifdef USE_IMGUI
	if (attackComponent_)
	{
		attackComponent_->DrawImGui();
	}
#endif
}

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void BossBase::Finalize()
{
	if (attackComponent_)
	{
		attackComponent_->Finalize();
		attackComponent_.reset();
	}

	if (animationComponent_)
	{
		animationComponent_->Finalize();
		animationComponent_.reset();
	}

	if (movementComponent_)
	{
		movementComponent_->Finalize();
		movementComponent_.reset();
	}

	if (stateMachine_)
	{
		stateMachine_->Finalize();
		stateMachine_.reset();
	}

	if (statusComponent_)
	{
		statusComponent_->Finalize();
		statusComponent_.reset();
	}

	if (brain_)
	{
		brain_->Finalize();
		brain_.reset();
	}

	//phaseComponent_.reset();
	//weakPointComponent_.reset();

	// BaseCharacter 側の body_ / parts_ は unique_ptr 管理なので、
	// 必要なら明示的にクリアしてもよい
	GetBodyParts().clear();
	GetBody().object.reset();
}

/// -------------------------------------------------------------
/// ダメージ
/// -------------------------------------------------------------
void BossBase::OnDamaged(float damage)
{
	if (!statusComponent_)
	{
		return;
	}

	statusComponent_->ApplyDamage(damage);
}

/// -------------------------------------------------------------
/// 死亡
/// -------------------------------------------------------------
void BossBase::OnDead()
{
	state_ = BossState::Dead;

	if (stateMachine_)
	{
		stateMachine_->ChangeState(*this, BossState::Dead);
	}

	if (attackComponent_)
	{
		attackComponent_->ForceEndCurrentAttack();
	}
}

/// -------------------------------------------------------------
/// 生死
/// -------------------------------------------------------------
bool BossBase::IsAlive() const
{
	return statusComponent_ ? statusComponent_->IsAlive() : false;
}

bool BossBase::IsDead() const
{
	return statusComponent_ ? statusComponent_->IsDead() : true;
}

/// -------------------------------------------------------------
/// HP参照
/// -------------------------------------------------------------
float BossBase::GetHP() const
{
	return statusComponent_ ? statusComponent_->GetHP() : 0.0f;
}

float BossBase::GetMaxHP() const
{
	return statusComponent_ ? statusComponent_->GetMaxHP() : 0.0f;
}

float BossBase::GetHPRate() const
{
	return statusComponent_ ? statusComponent_->GetHPRate() : 0.0f;
}

/// -------------------------------------------------------------
/// ターゲットまでのXZ距離
/// -------------------------------------------------------------
float BossBase::GetDistanceToTargetXZ() const
{
	const K4E::Vector3 from = GetPosition();
	const K4E::Vector3 to = targetPosition_;

	const float dx = to.x - from.x;
	const float dz = to.z - from.z;

	return std::sqrt(dx * dx + dz * dz);
}

/// -------------------------------------------------------------
/// ターゲットが攻撃範囲内か
/// -------------------------------------------------------------
bool BossBase::IsTargetInAttackRange() const
{
	return GetDistanceToTargetXZ() <= attackRange_;
}

/// -------------------------------------------------------------
/// 攻撃登録
/// -------------------------------------------------------------
void BossBase::RegisterAttack(std::unique_ptr<IBossAttack> attack)
{
	if (!attack)
	{
		return;
	}

	if (attackComponent_)
	{
		attackComponent_->RegisterAttack(std::move(attack));
	}
}

/// -------------------------------------------------------------
/// 共通更新群
/// 中身は後で肉付けしていく
/// -------------------------------------------------------------
void BossBase::UpdateState(float deltaTime)
{
	(void)deltaTime;

	if (!stateMachine_)
	{
		return;
	}

	// 死亡していたら Dead を優先
	if (IsDead())
	{
		stateMachine_->ChangeState(*this, BossState::Dead);
		state_ = stateMachine_->GetCurrentState();
		return;
	}

	switch (stateMachine_->GetCurrentState())
	{
	case BossState::Intro:
		// 仮: 登場演出後すぐ待機へ
		stateMachine_->ChangeState(*this, BossState::Idle);
		break;

	case BossState::Idle:
		// 攻撃範囲内 かつ クールタイムが終わっているなら攻撃へ
		if (IsTargetInAttackRange() && !IsAttackCoolingDown())
		{
			stateMachine_->ChangeState(*this, BossState::Attack);
		}
		else
		{
			stateMachine_->ChangeState(*this, BossState::Move);
		}
		break;

	case BossState::Move:
		// 攻撃範囲に入ったら攻撃へ
		if (IsTargetInAttackRange() && !IsAttackCoolingDown())
		{
			stateMachine_->ChangeState(*this, BossState::Attack);
		}
		break;

	case BossState::Attack:
		// 攻撃終了判定は AttackComponent 側を参照
		if (attackComponent_ && !attackComponent_->IsAttacking())
		{
			attackCooldownTimer_ = attackCooldownSec_;

			if (IsTargetInAttackRange())
			{
				stateMachine_->ChangeState(*this, BossState::Idle);
			}
			else
			{
				stateMachine_->ChangeState(*this, BossState::Move);
			}
		}
		break;

	case BossState::Stagger:
	case BossState::Down:
	case BossState::PhaseTransition:
	case BossState::Dead:
	default:
		break;
	}

	state_ = stateMachine_->GetCurrentState();
}

void BossBase::UpdatePhase(float deltaTime)
{
	(void)deltaTime;
}

void BossBase::UpdateMovement(float deltaTime)
{
	if (movementComponent_)
	{
		movementComponent_->Update(*this, deltaTime);
	}
}

void BossBase::UpdateAttack(float deltaTime)
{
	if (!attackComponent_)
	{
		return;
	}

	// ---------------------------------------------------------
	// 実行中攻撃の更新だけを担当する
	// 攻撃の「選択」は派生 Boss や Brain 側で行う
	// ここで勝手に startableAttacks[0] を始めると、
	// Guardian の判断と二重化してしまうのでやめる
	// ---------------------------------------------------------
	attackComponent_->Update(deltaTime);
}

void BossBase::UpdateWeakPoints(float deltaTime)
{
	(void)deltaTime;
}

void BossBase::CheckDeath()
{
	if (IsDead() && state_ != BossState::Dead)
	{
		OnDead();
	}
}

/// -------------------------------------------------------------
/// 指定部位のワールド座標を取得
/// BodyPart の transform を更新して worldTranslate_ を返す
/// -------------------------------------------------------------
K4E::Vector3 BossBase::GetPartWorldPosition(size_t partIndex)
{
	auto& parts = GetBodyParts();
	if (partIndex >= parts.size())
	{
		// 範囲外なら本体中心を返す
		return GetCenterPosition();
	}

	// 本体Transformを先に更新
	auto& body = GetBody();
	body.transform.Update();

	// 部位Transformを本体基準で更新
	auto& part = parts[partIndex];
	part.transform.worldRotate_ = body.transform.worldRotate_;
	part.transform.Update();

	return part.transform.worldTranslate_;
}

/// -------------------------------------------------------------
/// 簡易球ヒット判定
/// 攻撃球 と 対象球 が重なっているかを見る
/// -------------------------------------------------------------
bool BossBase::IsSphereHit(const K4E::Vector3& attackCenter, float attackRadius, const K4E::Vector3& targetCenter, float targetRadius) const
{
	const float dx = attackCenter.x - targetCenter.x;
	const float dy = attackCenter.y - targetCenter.y;
	const float dz = attackCenter.z - targetCenter.z;

	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const float sumRadius = attackRadius + targetRadius;

	return distanceSq <= (sumRadius * sumRadius);
}

/// -------------------------------------------------------------
/// デバッグ用の簡易球ヒット判定
/// 
/// 優先順位:
/// 1. 頭
/// 2. 胴体
/// 3. 腕
/// 4. 脚
/// 
/// まずは「当たった / 頭に当たった」確認用の簡易実装
/// 将来的には OBB や各部位 Collider に置き換えてOK
/// -------------------------------------------------------------
BossHitResult BossBase::CheckDebugHitSphere(const K4E::Vector3& attackCenter, float attackRadius)
{
	BossHitResult result{};

	// 死んでいたら判定しない
	if (IsDead())
	{
		return result;
	}

	// 部位インデックス取得
	const auto& indices = GetPartIndices();

	// 仮半径
	// 実モデルに合わせて後で調整する
	const float headRadius = 0.45f;
	const float bodyRadius = 0.85f;
	const float armRadius = 0.45f;
	const float legRadius = 0.50f;

	// --- 頭 ---
	{
		const K4E::Vector3 headPos = GetPartWorldPosition(indices.head);
		if (IsSphereHit(attackCenter, attackRadius, headPos, headRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::Head;
			result.hitPosition = headPos;
			result.damageMultiplier = 2.0f; // 頭は大きめダメージ
			return result;
		}
	}

	// --- 胴体 ---
	{
		const K4E::Vector3 bodyPos = GetCenterPosition();
		if (IsSphereHit(attackCenter, attackRadius, bodyPos, bodyRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::Body;
			result.hitPosition = bodyPos;
			result.damageMultiplier = 1.0f;
			return result;
		}
	}

	// --- 左腕 ---
	{
		const K4E::Vector3 pos = GetPartWorldPosition(indices.leftArm);
		if (IsSphereHit(attackCenter, attackRadius, pos, armRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::LeftArm;
			result.hitPosition = pos;
			result.damageMultiplier = 0.8f;
			return result;
		}
	}

	// --- 右腕 ---
	{
		const K4E::Vector3 pos = GetPartWorldPosition(indices.rightArm);
		if (IsSphereHit(attackCenter, attackRadius, pos, armRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::RightArm;
			result.hitPosition = pos;
			result.damageMultiplier = 0.8f;
			return result;
		}
	}

	// --- 左脚 ---
	{
		const K4E::Vector3 pos = GetPartWorldPosition(indices.leftLeg);
		if (IsSphereHit(attackCenter, attackRadius, pos, legRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::LeftLeg;
			result.hitPosition = pos;
			result.damageMultiplier = 0.9f;
			return result;
		}
	}

	// --- 右脚 ---
	{
		const K4E::Vector3 pos = GetPartWorldPosition(indices.rightLeg);
		if (IsSphereHit(attackCenter, attackRadius, pos, legRadius))
		{
			result.isHit = true;
			result.part = BossHitPart::RightLeg;
			result.hitPosition = pos;
			result.damageMultiplier = 0.9f;
			return result;
		}
	}

	// 何にも当たらなかった
	return result;
}

/// -------------------------------------------------------------
/// デバッグ用ヒット結果からダメージ適用
/// 
/// baseDamage に部位倍率を掛けて最終ダメージを決める
/// -------------------------------------------------------------
void BossBase::ApplyDebugHitResult(const BossHitResult& hitResult, float baseDamage)
{
	if (!hitResult.isHit)
	{
		return;
	}

	const float finalDamage = baseDamage * hitResult.damageMultiplier;
	OnDamaged(finalDamage);
}