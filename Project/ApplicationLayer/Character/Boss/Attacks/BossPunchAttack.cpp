#include "BossPunchAttack.h"
#include "Core/BossBase.h"

#include <Windows.h>
#include <cmath>
#include <string>

namespace
{
	void DebugLog(const std::string& text)
	{
		OutputDebugStringA(text.c_str());
	}
}

/// ---------------------------------------------------------------
/// 初期化
/// ---------------------------------------------------------------
void BossPunchAttack::Initialize(BossBase* owner)
{
	owner_ = owner;

	isActive_ = false;
	isFinished_ = false;
	hasHit_ = false;

	timer_ = 0.0f;
	cooldownRemaining_ = 0.0f;
}

/// ---------------------------------------------------------------
/// 攻撃開始
/// ---------------------------------------------------------------
void BossPunchAttack::Start()
{
	if (!CanStart())
	{
		return;
	}

	isActive_ = true;
	isFinished_ = false;
	hasHit_ = false;
	timer_ = 0.0f;

	DebugLog("[BossPunchAttack] Start\n");
}

/// ---------------------------------------------------------------
/// 攻撃更新
/// ---------------------------------------------------------------
void BossPunchAttack::Update(float deltaTime)
{
	if (!isActive_)
	{
		return;
	}

	timer_ += deltaTime;

	// ヒットタイミングで一度だけ判定発生
	if (!hasHit_ && timer_ >= hitTime_)
	{
		TryHitPlayer();
		hasHit_ = true;
	}

	// 終了タイミング
	if (timer_ >= recoveryEndTime_)
	{
		End();
	}
}

/// ---------------------------------------------------------------
/// 攻撃終了
/// ---------------------------------------------------------------
void BossPunchAttack::End()
{
	if (!isActive_)
	{
		return;
	}

	isActive_ = false;
	isFinished_ = true;
	cooldownRemaining_ = cooldownSec_;

	DebugLog("[BossPunchAttack] End\n");
}

/// ---------------------------------------------------------------
/// 今この攻撃を開始できるか
/// ---------------------------------------------------------------
bool BossPunchAttack::CanStart() const
{
	if (owner_ == nullptr)
	{
		return false;
	}

	if (isActive_)
	{
		return false;
	}

	if (cooldownRemaining_ > 0.0f)
	{
		return false;
	}

	if (owner_->IsDead())
	{
		return false;
	}

	if (!IsTargetInValidRange())
	{
		return false;
	}

	return true;
}

/// ---------------------------------------------------------------
/// クールダウン更新
/// ---------------------------------------------------------------
void BossPunchAttack::TickCooldown(float deltaTime)
{
	if (cooldownRemaining_ <= 0.0f)
	{
		return;
	}

	cooldownRemaining_ -= deltaTime;
	if (cooldownRemaining_ < 0.0f)
	{
		cooldownRemaining_ = 0.0f;
	}
}

/// ---------------------------------------------------------------
/// 描画
/// ---------------------------------------------------------------
void BossPunchAttack::Draw()
{
	// 必要なら予兆描画やヒット位置のデバッグ描画を行う
}

/// ---------------------------------------------------------------
/// ImGui
/// ---------------------------------------------------------------
void BossPunchAttack::DrawImGui()
{
	// 必要なら後で表示
	// 例:
	// ImGui::Text("Punch Active: %s", isActive_ ? "true" : "false");
}

/// ---------------------------------------------------------------
/// ヒット判定を一度だけ発生させる
/// ---------------------------------------------------------------
void BossPunchAttack::TryHitPlayer()
{
	if (owner_ == nullptr)
	{
		return;
	}

	// -----------------------------------------------------------
	// 攻撃中心を作る
	// 右腕根本のワールド座標を基準に、
	// 正面へ少し前に出した位置を攻撃中心にする
	// -----------------------------------------------------------
	const K4E::Vector3 armRoot = owner_->GetRightArmRootWorldPosition();

	const float yaw = owner_->GetYaw();

	K4E::Vector3 forward
	{
		std::sin(yaw),
		0.0f,
		std::cos(yaw)
	};

	K4E::Vector3 attackCenter = armRoot;
	attackCenter.x += forward.x * hitForwardOffset_;
	attackCenter.y += 0.25f;
	attackCenter.z += forward.z * hitForwardOffset_;

	// -----------------------------------------------------------
	// 仮のプレイヤー中心
	// 今は BossBase が持つ targetPosition_ を使う
	// -----------------------------------------------------------
	const K4E::Vector3 targetCenter = owner_->GetTargetPosition();
	const float targetRadius = 0.65f;

	const float dx = targetCenter.x - attackCenter.x;
	const float dy = targetCenter.y - attackCenter.y;
	const float dz = targetCenter.z - attackCenter.z;

	const float distanceSq = dx * dx + dy * dy + dz * dz;
	const float sumRadius = hitRadius_ + targetRadius;

	if (distanceSq <= (sumRadius * sumRadius))
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossPunchAttack] Melee hit success.\n");
#endif

		// TODO:
		// 将来的にはここでプレイヤーへダメージを与える
		// 例:
		// owner_->GetTargetPlayer()->OnDamaged(damage_);
	}
	else
	{
#ifdef _DEBUG
		OutputDebugStringA("[BossPunchAttack] Melee hit miss.\n");
#endif
	}
}

/// ---------------------------------------------------------------
/// 有効距離内か
/// ---------------------------------------------------------------
bool BossPunchAttack::IsTargetInValidRange() const
{
	if (owner_ == nullptr)
	{
		return false;
	}

	const float distance = owner_->GetDistanceToTargetXZ();
	return (distance >= minRange_ && distance <= maxRange_);
}