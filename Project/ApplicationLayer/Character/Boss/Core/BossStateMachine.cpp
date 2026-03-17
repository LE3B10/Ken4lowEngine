#include "BossStateMachine.h"
#include "BossBase.h"

#include <Windows.h>
#include <string>

namespace
{
	void DebugLog(const std::string& text)
	{
		OutputDebugStringA(text.c_str());
	}
}

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void BossStateMachine::Initialize(BossState initialState)
{
	currentState_ = initialState;
	prevState_ = initialState;
	stateTime_ = 0.0f;
}

/// -------------------------------------------------------------
/// 更新
/// -------------------------------------------------------------
void BossStateMachine::Update(BossBase& boss, float deltaTime)
{
	stateTime_ += deltaTime;
	OnUpdate(boss, currentState_, deltaTime);
}

/// -------------------------------------------------------------
/// 状態変更
/// -------------------------------------------------------------
void BossStateMachine::ChangeState(BossBase& boss, BossState newState)
{
	// 同じ状態なら何もしない
	if (currentState_ == newState)
	{
		return;
	}

	OnExit(boss, currentState_);

	prevState_ = currentState_;
	currentState_ = newState;
	stateTime_ = 0.0f;

	OnEnter(boss, currentState_);
}

/// -------------------------------------------------------------
/// 状態に入った瞬間
/// -------------------------------------------------------------
void BossStateMachine::OnEnter(BossBase& boss, BossState state)
{
	(void)boss;

	switch (state)
	{
	case BossState::Intro:
		DebugLog("[BossStateMachine] Enter Intro\n");
		break;

	case BossState::Idle:
		DebugLog("[BossStateMachine] Enter Idle\n");
		break;

	case BossState::Move:
		DebugLog("[BossStateMachine] Enter Move\n");
		break;

	case BossState::Attack:
		DebugLog("[BossStateMachine] Enter Attack\n");
		break;

	case BossState::Stagger:
		DebugLog("[BossStateMachine] Enter Stagger\n");
		break;

	case BossState::Down:
		DebugLog("[BossStateMachine] Enter Down\n");
		break;

	case BossState::PhaseTransition:
		DebugLog("[BossStateMachine] Enter PhaseTransition\n");
		break;

	case BossState::Dead:
		DebugLog("[BossStateMachine] Enter Dead\n");
		break;

	default:
		break;
	}
}

/// -------------------------------------------------------------
/// 状態を抜ける瞬間
/// -------------------------------------------------------------
void BossStateMachine::OnExit(BossBase& boss, BossState state)
{
	(void)boss;
	(void)state;
}

/// -------------------------------------------------------------
/// 状態中更新
/// -------------------------------------------------------------
void BossStateMachine::OnUpdate(BossBase& boss, BossState state, float deltaTime)
{
	(void)boss;
	(void)deltaTime;

	switch (state)
	{
	case BossState::Intro:
	case BossState::Idle:
	case BossState::Move:
	case BossState::Attack:
	case BossState::Stagger:
	case BossState::Down:
	case BossState::PhaseTransition:
	case BossState::Dead:
	default:
		break;
	}
}