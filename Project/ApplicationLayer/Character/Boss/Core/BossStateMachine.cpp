#include "BossStateMachine.h"
#include "BossBase.h"

#ifdef _DEBUG
#include <LogString.h>
#endif // _DEBUG

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///						　初期化処理
/// -------------------------------------------------------------
void BossStateMachine::Initialize(BossState initialState)
{
	currentState_ = initialState;
	prevState_ = initialState;
	stateTime_ = 0.0f;
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void BossStateMachine::Update(BossBase& boss, float deltaTime)
{
	stateTime_ += deltaTime;
	OnUpdate(boss, currentState_, deltaTime);
}

/// -------------------------------------------------------------
///							状態変更
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
///						状態に入った瞬間
/// -------------------------------------------------------------
void BossStateMachine::OnEnter(BossBase& boss, BossState state)
{
	(void)boss;

	switch (state)
	{
	case BossState::Intro:
		Log("[BossStateMachine] Enter Intro\n");
		break;

	case BossState::Idle:
		Log("[BossStateMachine] Enter Idle\n");
		break;

	case BossState::Move:
		Log("[BossStateMachine] Enter Move\n");
		break;

	case BossState::Attack:
		Log("[BossStateMachine] Enter Attack\n");
		break;

	case BossState::Stagger:
		Log("[BossStateMachine] Enter Stagger\n");
		break;

	case BossState::Down:
		Log("[BossStateMachine] Enter Down\n");
		break;

	case BossState::PhaseTransition:
		Log("[BossStateMachine] Enter PhaseTransition\n");
		break;

	case BossState::Dead:
		Log("[BossStateMachine] Enter Dead\n");
		break;

	default:
		break;
	}
}

/// -------------------------------------------------------------
///						状態を抜ける瞬間
/// -------------------------------------------------------------
void BossStateMachine::OnExit(BossBase& boss, BossState state)
{
	(void)boss;
	(void)state;
}

/// -------------------------------------------------------------
///							状態中更新
/// -------------------------------------------------------------
void BossStateMachine::OnUpdate(BossBase& boss, BossState state, float deltaTime)
{
	(void)boss;
	(void)deltaTime;

	switch (state)
	{
	case BossState::Intro:			 // 登場演出中
	case BossState::Idle:			 // 待機
	case BossState::Move:			 // 移動
	case BossState::Attack:			 // 攻撃
	case BossState::Stagger:		 // タイミングミス
	case BossState::Down:			 // 倒れ
	case BossState::PhaseTransition: // フェーズ遷移
	case BossState::Dead:			 // 死亡
	default:
		break;
	}
}