#define NOMINMAX
#include "ModelBlockEffectSequence.h"

#include "ModelDisintegrationEffect.h"
#include "ModelReconstructionEffect.h"

#include <algorithm>

void ModelBlockEffectSequence::Initialize(ModelReconstructionEffect* reconstructionEffect, ModelDisintegrationEffect* disintegrationEffect)
{
	reconstructionEffect_ = reconstructionEffect;
	disintegrationEffect_ = disintegrationEffect;
	Reset();
}

void ModelBlockEffectSequence::PlaySpawnThenDisintegrate(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::SpawnThenDisintegrate, parameters_.loopEnabled);
	StartReconstruction();
}

void ModelBlockEffectSequence::PlayDisintegrateThenReconstruct(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::DisintegrateThenReconstruct, parameters_.loopEnabled);
	modelVisible_ = true;
	StartDisintegration();
}

void ModelBlockEffectSequence::PlayLoop(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix)
{
	Begin(modelPath, worldMatrix, SequenceMode::SpawnThenDisintegrate, true);
	StartReconstruction();
}

void ModelBlockEffectSequence::Stop(bool showModelAfterStop)
{
	ResetEffects();
	modelVisible_ = showModelAfterStop;
	state_ = SequenceState::Idle;
	waitingReason_ = WaitingReason::None;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
}

void ModelBlockEffectSequence::Reset()
{
	ResetEffects();
	modelVisible_ = true;
	state_ = SequenceState::Idle;
	mode_ = SequenceMode::SpawnThenDisintegrate;
	waitingReason_ = WaitingReason::None;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
	reconstructionCompleted_ = false;
	disintegrationCompleted_ = false;
}

void ModelBlockEffectSequence::Update(float deltaTime)
{
	if (state_ == SequenceState::Idle || state_ == SequenceState::Completed)
	{
		return;
	}

	sequenceElapsed_ += deltaTime;
	stateElapsed_ += deltaTime;

	if (reconstructionEffect_)
	{
		reconstructionEffect_->Update(deltaTime);
	}
	if (disintegrationEffect_)
	{
		disintegrationEffect_->Update(deltaTime);
	}

	switch (state_)
	{
	case SequenceState::Reconstructing:
		if (reconstructionEffect_ && reconstructionEffect_->IsComplete())
		{
			reconstructionCompleted_ = true;
			modelVisible_ = true;
			if (mode_ == SequenceMode::DisintegrateThenReconstruct && !parameters_.loopEnabled)
			{
				ResetEffects();
				EnterState(SequenceState::Completed);
			}
			else
			{
				EnterState(SequenceState::ShowingModel);
			}
		}
		break;
	case SequenceState::ShowingModel:
		if (stateElapsed_ >= std::max(parameters_.showDuration, 0.0f))
		{
			StartDisintegration();
		}
		break;
	case SequenceState::Disintegrating:
		if (disintegrationEffect_ && !disintegrationEffect_->IsActive())
		{
			disintegrationCompleted_ = true;
			modelVisible_ = false;
			if (mode_ == SequenceMode::DisintegrateThenReconstruct)
			{
				waitingReason_ = WaitingReason::BeforeReconstruct;
				EnterState(SequenceState::Waiting);
			}
			else
			{
				CompleteOrLoop();
			}
		}
		break;
	case SequenceState::Waiting:
		if (stateElapsed_ >= std::max(parameters_.waitDuration, 0.0f))
		{
			if (waitingReason_ == WaitingReason::BeforeReconstruct)
			{
				StartReconstruction();
			}
			else if (waitingReason_ == WaitingReason::BeforeLoopRestart)
			{
				StartReconstruction();
			}
		}
		break;
	default:
		break;
	}
}

const char* ModelBlockEffectSequence::GetStateName() const
{
	switch (state_)
	{
	case SequenceState::Idle: return "待機中";
	case SequenceState::Reconstructing: return "再構築中";
	case SequenceState::ShowingModel: return "通常モデル表示中";
	case SequenceState::Disintegrating: return "崩壊中";
	case SequenceState::Waiting: return "待機中";
	case SequenceState::Completed: return "完了";
	default: return "不明";
	}
}

void ModelBlockEffectSequence::Begin(const std::string& modelPath, const K4E::Matrix4x4& worldMatrix, SequenceMode mode, bool loopEnabled)
{
	modelPath_ = modelPath;
	worldMatrix_ = worldMatrix;
	mode_ = mode;
	parameters_.loopEnabled = loopEnabled;
	sequenceElapsed_ = 0.0f;
	stateElapsed_ = 0.0f;
	reconstructionCompleted_ = false;
	disintegrationCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
}

void ModelBlockEffectSequence::ResetEffects()
{
	if (reconstructionEffect_)
	{
		reconstructionEffect_->Initialize();
	}
	if (disintegrationEffect_)
	{
		disintegrationEffect_->Initialize();
	}
}

void ModelBlockEffectSequence::ApplySharedParameters()
{
	if (reconstructionEffect_)
	{
		auto& reconstructionParams = reconstructionEffect_->GetParameters();
		reconstructionParams.blockCount = parameters_.blockCount;
		reconstructionParams.blockSize = parameters_.blockSize;
		reconstructionParams.surfaceSampling = parameters_.surfaceSampling;
		reconstructionParams.showFinalModel = false;
		reconstructionParams.autoHideBlocksAfterComplete = true;
	}

	if (disintegrationEffect_)
	{
		auto& disintegrationParams = disintegrationEffect_->GetParameters();
		disintegrationParams.particleCount = parameters_.blockCount;
		disintegrationParams.particleSize = parameters_.blockSize;
		disintegrationParams.blockSize = parameters_.blockSize;
		disintegrationParams.surfaceSampling = parameters_.surfaceSampling;
	}
}

void ModelBlockEffectSequence::StartReconstruction()
{
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
	reconstructionCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Reconstructing);
	if (reconstructionEffect_)
	{
		// 再構築中は通常モデルを隠し、ブロックだけで登場演出を確認する。
		reconstructionEffect_->PlayFromModel(modelPath_, worldMatrix_);
	}
}

void ModelBlockEffectSequence::StartDisintegration()
{
	ResetEffects();
	ApplySharedParameters();
	modelVisible_ = false;
	disintegrationCompleted_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Disintegrating);
	if (disintegrationEffect_)
	{
		disintegrationEffect_->PlayFromModel(modelPath_, worldMatrix_);
	}
}

void ModelBlockEffectSequence::EnterState(SequenceState nextState)
{
	state_ = nextState;
	stateElapsed_ = 0.0f;
}

void ModelBlockEffectSequence::CompleteOrLoop()
{
	if (parameters_.loopEnabled)
	{
		waitingReason_ = WaitingReason::BeforeLoopRestart;
		EnterState(SequenceState::Waiting);
		return;
	}

	ResetEffects();
	modelVisible_ = false;
	waitingReason_ = WaitingReason::None;
	EnterState(SequenceState::Completed);
}
