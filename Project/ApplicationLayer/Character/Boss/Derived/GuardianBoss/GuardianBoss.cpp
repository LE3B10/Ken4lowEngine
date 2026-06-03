#define NOMINMAX
#include "GuardianBoss.h"
#include "BossPunchAttack.h"
#include "BossHeavyPunchAttack.h"
#include <LinearInterpolation.h>
#include <LogString.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
/// Guardian 固有初期化
/// -------------------------------------------------------------
void GuardianBoss::SetupBoss()
{
	// 人型ボス共通初期化
	HumanoidBossBase::SetupBoss();

	// フェーズ初期化
	SetPhase(BossPhase::Phase1);

	stateTimer_ = 0.0f;
	attackCooldownTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;

	// HeavyPunch 連打抑制初期化
	lastSelectedAttack_ = "None";
	heavyPunchReuseTimer_ = 0.0f;

	// ---------------------------------------------------------
	// 初期状態は Idle
	// ここも StateMachine 経由で合わせる
	// ---------------------------------------------------------
	ChangeBossState(BossState::Idle);

	// ---------------------------------------------------------
	// アニメーションコンポーネントへ Guardian 用パラメータを渡す
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->SetWalkSpeed(6.0f);
		GetAnimationComponent()->SetWalkAmplitude(0.55f);
		GetAnimationComponent()->SetAttackDuration(attackDuration_);

		GetAnimationComponent()->ResetWalkTimer();
		GetAnimationComponent()->ResetAttackTimer();
		GetAnimationComponent()->ResetAllPose(1.0f);
	}

	// スキン一括適用
	ApplySkinToAllParts(GetGuardianSkinPath());
}

/// -------------------------------------------------------------
/// ダメージ
/// 軽いひるみへ移行
/// -------------------------------------------------------------
void GuardianBoss::OnDamaged(float damage)
{
	if (GetState() == BossState::Dead)
	{
		return;
	}

	const float hpBefore = GetHP();

	// HP減算は基底側に任せる
	BossBase::OnDamaged(damage);

	if (GetHP() < hpBefore)
	{
		++receivedHitCount_;
		lastReceivedDamage_ = damage;
	}

	{
		std::ostringstream oss;
		oss << "[GuardianBoss] HP=" << GetHP() << "/" << GetMaxHP()
			<< ", hitCount=" << receivedHitCount_
			<< ", lastDamage=" << lastReceivedDamage_;
		Log(oss.str() + "\n");
	}

	// 生きていたらひるみへ
	if (!IsDead())
	{
		BeginStaggerState();
	}
}

/// -------------------------------------------------------------
/// 死亡
/// -------------------------------------------------------------
void GuardianBoss::OnDead()
{
	ChangeBossState(BossState::Dead);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAllPose(1.0f);
	}
}

/// -------------------------------------------------------------
/// 衝突
/// 今は空実装
/// -------------------------------------------------------------
void GuardianBoss::OnCollision(Collider* other)
{
	(void)other;
}

/// -------------------------------------------------------------
/// 状態更新
/// Guardian の思考をここで決める
/// -------------------------------------------------------------
void GuardianBoss::UpdateState(float deltaTime)
{
	stateTimer_ += deltaTime;
	attackCooldownTimer_ = std::max(0.0f, attackCooldownTimer_ - deltaTime);

	// ---------------------------------------------------------
	// HeavyPunch の連打抑制タイマー
	// ---------------------------------------------------------
	heavyPunchReuseTimer_ = std::max(0.0f, heavyPunchReuseTimer_ - deltaTime);

	CheckDeath();
	if (GetState() == BossState::Dead)
	{
		return;
	}

	switch (GetState())
	{
	case BossState::Intro:
		{
			BeginIdleState();
			break;
		}

	case BossState::Idle:
		{
			const float distance = GetDistanceToTargetXZ();

			// ---------------------------------------------------------
			// クールタイム中は完全停止
			// ・移動しない
			// ・向き直りもしない
			// ・攻撃もしない
			// ---------------------------------------------------------
			if (attackCooldownTimer_ > 0.0f)
			{
				break;
			}

			FaceTarget(deltaTime);

			// 攻撃条件成立
			if (distance <= attackRange_)
			{
				BeginAttackState();
			}
			// 遠ければ移動へ
			else if (distance > moveStartDistance_)
			{
				BeginMoveState();
			}
			break;
		}

	case BossState::Move:
		{
			FaceTarget(deltaTime);

			const float distance = GetDistanceToTargetXZ();

			// 攻撃条件成立
			if (distance <= attackRange_ && attackCooldownTimer_ <= 0.0f)
			{
				BeginAttackState();
			}
			// 十分近づいたら待機
			else if (distance <= moveStartDistance_)
			{
				BeginIdleState();
			}
			break;
		}

	case BossState::Attack:
		{
			FaceTarget(deltaTime);

			// ---------------------------------------------------------
			// 手動デバッグ中でない場合のみ、自動で攻撃を選ぶ
			// ---------------------------------------------------------
			if (!useManualAttackDebug_)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					if (stateTimer_ <= 0.10f)
					{
						TryStartBestAttack();
					}
				}
			}

			// ---------------------------------------------------------
			// 少し待ってから攻撃終了判定
			// 手動デバッグ中でも、攻撃が終わったら Idle に戻す
			// ---------------------------------------------------------
			if (stateTimer_ >= 0.05f)
			{
				if (GetAttackComponent() && !GetAttackComponent()->IsAttacking())
				{
					attackCooldownTimer_ = attackCooldown_;
					hasAppliedAttackHit_ = false;
					BeginIdleState();
				}
			}
			break;
		}

	case BossState::Stagger:
		{
			if (stateTimer_ >= staggerDuration_)
			{
				BeginIdleState();
			}
			break;
		}

	case BossState::Down:
	case BossState::PhaseTransition:
	case BossState::Dead:
	default:
		{
			break;
		}
	}
}

/// -------------------------------------------------------------
/// 移動更新
/// Move状態のときだけ前進する
/// -------------------------------------------------------------
void GuardianBoss::UpdateMovement(float deltaTime)
{
	if (GetState() != BossState::Move)
	{
		return;
	}

	// 攻撃直後のクールタイム中は絶対に動かない
	if (attackCooldownTimer_ > 0.0f)
	{
		return;
	}

	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float len = std::sqrt(lenSq);
	toTarget.x /= len;
	toTarget.z /= len;

	Vector3 newPos = GetPosition();
	newPos.x += toTarget.x * moveSpeed_ * deltaTime;
	newPos.z += toTarget.z * moveSpeed_ * deltaTime;
	SetPosition(newPos);
}

/// -------------------------------------------------------------
/// 攻撃更新
/// 実際の攻撃処理は基底側 + AttackComponent に任せる
/// 見た目アニメは AnimationComponent 側で更新される
/// -------------------------------------------------------------
void GuardianBoss::UpdateAttack(float deltaTime)
{
	BossBase::UpdateAttack(deltaTime);
	(void)deltaTime;
}

/// -------------------------------------------------------------
/// 死亡チェック
/// -------------------------------------------------------------
void GuardianBoss::CheckDeath()
{
	if (IsDead() && GetState() != BossState::Dead)
	{
		OnDead();
	}
}

/// -------------------------------------------------------------
/// 攻撃登録
/// -------------------------------------------------------------
void GuardianBoss::SetupAttacks()
{
	RegisterAttack(std::make_unique<BossPunchAttack>());
	RegisterAttack(std::make_unique<BossHeavyPunchAttack>());
}

/// -------------------------------------------------------------
/// フェーズ設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupPhaseData()
{
}

/// -------------------------------------------------------------
/// 弱点設定
/// 今は空でOK
/// -------------------------------------------------------------
void GuardianBoss::SetupWeakPoints()
{
}

/// -------------------------------------------------------------
/// ターゲット方向へ向く
/// -------------------------------------------------------------
void GuardianBoss::FaceTarget(float deltaTime)
{
	Vector3 toTarget
	{
		GetTargetPosition().x - GetPosition().x,
		0.0f,
		GetTargetPosition().z - GetPosition().z
	};

	const float lenSq = toTarget.x * toTarget.x + toTarget.z * toTarget.z;
	if (lenSq <= 0.0001f)
	{
		return;
	}

	const float desiredYaw = std::atan2(-toTarget.x, toTarget.z);
	float currentYaw = GetYaw();

	float diff = WrapAngle(desiredYaw - currentYaw);
	const float maxStep = rotateSpeed_ * deltaTime;
	diff = std::clamp(diff, -maxStep, maxStep);

	currentYaw += diff;
	SetYaw(currentYaw);
}

/// -------------------------------------------------------------
/// ターゲットまでのXZ距離
/// -------------------------------------------------------------
float GuardianBoss::GetDistanceToTargetXZ() const
{
	const Vector3 pos = GetPosition();
	const Vector3 target = GetTargetPosition();

	const float dx = target.x - pos.x;
	const float dz = target.z - pos.z;
	return std::sqrt(dx * dx + dz * dz);
}

/// -------------------------------------------------------------
/// 状態変更ヘルパー
/// StateMachine と BossBase::state_ のズレを防ぐため、
/// 状態変更は必ずここを通す
/// -------------------------------------------------------------
void GuardianBoss::ChangeBossState(BossState newState)
{
	if (GetStateMachine())
	{
		GetStateMachine()->ChangeState(*this, newState);
	}
	else
	{
		// 念のためフォールバック
		SetState(newState);
	}
}

/// -------------------------------------------------------------
/// Attack 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginAttackState()
{
	ChangeBossState(BossState::Attack);
	stateTimer_ = 0.0f;
	hasAppliedAttackHit_ = false;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}

	// ---------------------------------------------------------
	// 手動デバッグ中はここで自動開始しない
	// ImGuiから手動で開始させる
	// ---------------------------------------------------------
	if (!useManualAttackDebug_)
	{
		TryStartBestAttack();
	}
}

/// -------------------------------------------------------------
/// Move 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginMoveState()
{
	ChangeBossState(BossState::Move);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetWalkTimer();
	}
}

/// -------------------------------------------------------------
/// Idle 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginIdleState()
{
	ChangeBossState(BossState::Idle);
	stateTimer_ = 0.0f;

	if (GetAnimationComponent())
	{
		// 歩行アニメの残りを消す
		GetAnimationComponent()->ResetWalkTimer();

		// 姿勢を自然に戻す
		GetAnimationComponent()->ResetAllPose(0.18f);
	}
}

/// -------------------------------------------------------------
/// Stagger 開始時共通処理
/// -------------------------------------------------------------
void GuardianBoss::BeginStaggerState()
{
	ChangeBossState(BossState::Stagger);
	stateTimer_ = 0.0f;
}

/// -------------------------------------------------------------
/// 攻撃ヒットタイミング
/// 今は将来拡張用に残す
/// 現段階では BossPunchAttack 側に判定を寄せる方針
/// -------------------------------------------------------------
void GuardianBoss::TryAttackHit()
{
	// 今は未使用
}

bool GuardianBoss::TryStartBestAttack()
{
	// AttackComponent が無いなら何もできない
	if (!GetAttackComponent())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// すでに攻撃中なら新規開始しない
	if (GetAttackComponent()->IsAttacking())
	{
		return false;
	}

	// Brain が無いなら判断できない
	if (!GetBrain())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 攻撃選択は Brain に任せる
	// ---------------------------------------------------------
	const std::string selectedAttack = GetBrain()->SelectBestAttackName();
	if (selectedAttack.empty())
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	// ---------------------------------------------------------
	// 実際の開始は Guardian 側で安全に行う
	// ここを通すことで Attack アニメ時間もリセットできる
	// ---------------------------------------------------------
	if (!StartAttackByNameSafe(selectedAttack.c_str()))
	{
		lastSelectedAttack_ = "None";
		return false;
	}

	lastSelectedAttack_ = selectedAttack;

	// HeavyPunch だけ軽い再使用待ちを残す
	if (selectedAttack == "HeavyPunch")
	{
		heavyPunchReuseTimer_ = heavyPunchReuseDelay_;
	}

	return true;
}

bool GuardianBoss::StartAttackByNameSafe(const char* attackName)
{
	if (!GetAttackComponent())
	{
		return false;
	}

	// すでに攻撃中なら新規開始しない
	if (GetAttackComponent()->IsAttacking())
	{
		return false;
	}

	// 指定名の攻撃を開始
	if (!GetAttackComponent()->StartAttackByName(attackName))
	{
		return false;
	}

	// 攻撃アニメ時間をリセット
	if (GetAnimationComponent())
	{
		GetAnimationComponent()->ResetAttackTimer();
	}

	return true;
}

/// -------------------------------------------------------------
/// ImGui
/// -------------------------------------------------------------
void GuardianBoss::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("GuardianBoss");

	// ---------------------------------------------------------
	// 基本状態
	// ---------------------------------------------------------
	ImGui::Text("State: %d", static_cast<int>(GetState()));
	ImGui::Text("HP: %.1f / %.1f", GetHP(), GetMaxHP());
	ImGui::Text("DistanceToTargetXZ: %.2f", GetDistanceToTargetXZ());

	ImGui::SeparatorText("ボス被弾確認");
	ImGui::Text("ボス出現済み: はい");
	ImGui::Text("ボス生存中: %s", IsAlive() ? "はい" : "いいえ");
	ImGui::Text("ボスHP: %.1f", GetHP());
	ImGui::Text("ボス最大HP: %.1f", GetMaxHP());
	ImGui::Text("ボスHP割合: %.1f%%", GetHPRate() * 100.0f);
	ImGui::Text("ボス被弾回数: %d", receivedHitCount_);
	ImGui::Text("最後にボスへ与えたダメージ: %.1f", lastReceivedDamage_);

	ImGui::Separator();
	ImGui::Text("StateTimer      : %.2f", stateTimer_);
	ImGui::Text("AttackCooldown  : %.2f", attackCooldownTimer_);
	ImGui::Text("IsCoolingDown   : %s", (attackCooldownTimer_ > 0.0f) ? "true" : "false");

	// ---------------------------------------------------------
	// 調整パラメータ
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Tuning");

	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.01f, 0.1f, 20.0f);
	ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.01f, 0.1f, 20.0f);
	ImGui::DragFloat("Move Start Dist", &moveStartDistance_, 0.01f, 0.1f, 50.0f);
	ImGui::DragFloat("Attack Range", &attackRange_, 0.01f, 0.1f, 20.0f);
	ImGui::DragFloat("Attack Duration", &attackDuration_, 0.01f, 0.05f, 10.0f);
	ImGui::DragFloat("Attack Cooldown", &attackCooldown_, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Stagger Duration", &staggerDuration_, 0.01f, 0.0f, 10.0f);

	// ---------------------------------------------------------
	// Guardian 専用補助情報
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Attack Context");

	ImGui::Text("LastSelectedAttack : %s", lastSelectedAttack_.c_str());
	ImGui::Text("HeavyReuseTimer    : %.2f", heavyPunchReuseTimer_);

	// ---------------------------------------------------------
	// Brain の判断確認
	// BossBase に brain_ / GetBrain() を追加した前提
	// ---------------------------------------------------------
	if (GetBrain())
	{
		ImGui::Separator();
		ImGui::Text("BossBrain Debug");

		ImGui::Text("BrainBestAttack : %s", GetBrain()->GetLastBestAttackName().c_str());
		ImGui::Text("BrainBestScore  : %.2f", GetBrain()->GetLastBestScore());
	}
	else
	{
		ImGui::Separator();
		ImGui::Text("BossBrain Debug");
		ImGui::Text("Brain : None");
	}

	// ---------------------------------------------------------
	// 手動攻撃デバッグ
	// useManualAttackDebug_ が true の間は、
	// BeginAttackState / UpdateState 側でも AI 自動選択を止める前提
	// ---------------------------------------------------------
	ImGui::Separator();
	ImGui::Text("Guardian Manual Attack Debug");

	ImGui::Checkbox("Use Manual Attack Debug", &useManualAttackDebug_);

	const char* attackItems[] =
	{
		"Punch",
		"HeavyPunch"
	};
	ImGui::Combo("Manual Attack", &manualAttackIndex_, attackItems, IM_ARRAYSIZE(attackItems));

	if (GetAttackComponent())
	{
		const bool isAttackState = (GetState() == BossState::Attack);
		const bool isAlreadyAttacking = GetAttackComponent()->IsAttacking();
		const bool canManualTrigger = isAttackState && !isAlreadyAttacking;

		ImGui::Text("ManualTriggerReady : %s", canManualTrigger ? "true" : "false");

		if (!isAttackState)
		{
			ImGui::Text("Note: Manual start is enabled only in Attack state.");
		}

		if (isAlreadyAttacking)
		{
			ImGui::Text("Note: Current attack is running.");
		}

		if (!canManualTrigger)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Start Selected Attack"))
		{
			if (manualAttackIndex_ == 0)
			{
				if (StartAttackByNameSafe("Punch"))
				{
					lastSelectedAttack_ = "Punch";
				}
			}
			else if (manualAttackIndex_ == 1)
			{
				if (StartAttackByNameSafe("HeavyPunch"))
				{
					lastSelectedAttack_ = "HeavyPunch";
					heavyPunchReuseTimer_ = heavyPunchReuseDelay_;
				}
			}
		}

		if (!canManualTrigger)
		{
			ImGui::EndDisabled();
		}

		// -----------------------------------------------------
		// Idle / Move からでもテストしやすくする
		// -----------------------------------------------------
		if (GetState() != BossState::Attack)
		{
			if (ImGui::Button("Force Enter Attack State"))
			{
				BeginAttackState();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Force Idle State"))
		{
			BeginIdleState();
		}
	}

	// ---------------------------------------------------------
	// Guardian 側の攻撃選択確認
	// priority / CanStart / 各種条件を見える化
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		ImGui::Text("Guardian Attack Selection Debug");

		IBossAttack* punch = GetAttackComponent()->FindAttackByName("Punch");
		IBossAttack* heavy = GetAttackComponent()->FindAttackByName("HeavyPunch");

		ImGui::Text("LastSelectedAttack : %s", lastSelectedAttack_.c_str());
		ImGui::Text("HeavyReuseTimer    : %.2f", heavyPunchReuseTimer_);
		ImGui::Text("DistanceToTargetXZ : %.2f", GetDistanceToTargetXZ());

		if (punch)
		{
			ImGui::Separator();
			ImGui::Text("[Punch]");
			ImGui::Text("Priority           : %d", punch->GetPriority());
			ImGui::Text("CanStart(Attack)   : %s", punch->CanStart() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", punch->GetCooldownRemaining());
			ImGui::Text("Range              : %.2f - %.2f", punch->GetMinRange(), punch->GetMaxRange());
		}
		else
		{
			ImGui::Text("[Punch] Not Registered");
		}

		if (heavy)
		{
			ImGui::Separator();
			ImGui::Text("[HeavyPunch]");
			ImGui::Text("Priority           : %d", heavy->GetPriority());
			ImGui::Text("CanStart(Attack)   : %s", heavy->CanStart() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", heavy->GetCooldownRemaining());
			ImGui::Text("Range              : %.2f - %.2f", heavy->GetMinRange(), heavy->GetMaxRange());

			// HeavyPunch が Guardian 側で落ちる理由
			ImGui::Text("HeavyDistanceOK    : %s", (GetDistanceToTargetXZ() <= 2.8f) ? "true" : "false");
			ImGui::Text("HeavyReuseOK       : %s", (heavyPunchReuseTimer_ <= 0.0f) ? "true" : "false");
			ImGui::Text("HeavyLastAttackOK  : %s", (lastSelectedAttack_ != "HeavyPunch") ? "true" : "false");
		}
		else
		{
			ImGui::Text("[HeavyPunch] Not Registered");
		}
	}

	// ---------------------------------------------------------
	// 現在攻撃中の詳細
	// Punch / HeavyPunch のフェーズ確認
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		ImGui::Text("Current Attack Debug");

		ImGui::Text("IsAttacking: %s", GetAttackComponent()->IsAttacking() ? "true" : "false");

		if (IBossAttack* current = GetAttackComponent()->GetCurrentAttack())
		{
			ImGui::Text("CurrentAttack      : %s", current->GetName());
			ImGui::Text("AttackFinished     : %s", current->IsFinished() ? "true" : "false");
			ImGui::Text("CooldownRemaining  : %.2f", current->GetCooldownRemaining());

			if (auto* punch = dynamic_cast<BossPunchAttack*>(current))
			{
				ImGui::Text("PunchPhase         : %d", static_cast<int>(punch->GetPhase()));
				ImGui::Text("PunchPhaseTimer    : %.2f", punch->GetPhaseTimer());
				ImGui::Text("PunchHasHit        : %s", punch->HasHit() ? "true" : "false");
			}

			if (auto* heavy = dynamic_cast<BossHeavyPunchAttack*>(current))
			{
				ImGui::Text("HeavyPhase         : %d", static_cast<int>(heavy->GetPhase()));
				ImGui::Text("HeavyPhaseTimer    : %.2f", heavy->GetPhaseTimer());
				ImGui::Text("HeavyHasHit        : %s", heavy->HasHit() ? "true" : "false");
			}
		}
		else
		{
			ImGui::Text("CurrentAttack      : None");
		}
	}

	// ---------------------------------------------------------
	// アニメーション確認
	// ---------------------------------------------------------
	if (GetAnimationComponent())
	{
		ImGui::Separator();
		ImGui::Text("Animation Debug");

		ImGui::Text("WalkAnimTime   : %.2f", GetAnimationComponent()->GetWalkTime());
		ImGui::Text("AttackAnimTime : %.2f", GetAnimationComponent()->GetAttackTime());
	}

	// ---------------------------------------------------------
	// AttackComponent 側詳細
	// 各攻撃の CanStart / Cooldown / Priority を見る
	// ---------------------------------------------------------
	if (GetAttackComponent())
	{
		ImGui::Separator();
		GetAttackComponent()->DrawImGui();
	}

	ImGui::End();
#endif
}