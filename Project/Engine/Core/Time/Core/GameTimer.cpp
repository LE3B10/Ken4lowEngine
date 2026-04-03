#include "GameTimer.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                     シングルトン取得
	/// -------------------------------------------------------------
	GameTimer* GameTimer::GetInstance()
	{
		static GameTimer instance;
		return &instance;
	}

	/// -------------------------------------------------------------
	///                         初期化
	/// -------------------------------------------------------------
	void GameTimer::Initialize(int targetFPS)
	{
		targetFPS_ = targetFPS;
		fpsCounter_ .Reset(targetFPS_);
		initialized_ = true;
	}

	/// -------------------------------------------------------------
	///                         終了処理
	/// -------------------------------------------------------------
	void GameTimer::Finalize()
	{
		initialized_ = false;
	}

	/// -------------------------------------------------------------
	///                     フレーム開始
	/// -------------------------------------------------------------
	void GameTimer::BeginFrame()
	{
		if (!initialized_)
		{
			Initialize(targetFPS_);
		}
		fpsCounter_.StartFrame();
	}

	/// -------------------------------------------------------------
	///                     フレーム終了
	/// -------------------------------------------------------------
	void GameTimer::EndFrame()
	{
		if (!initialized_)
		{
			return;
		}
		fpsCounter_.EndFrame();
	}

} // namespace Ken4lowEngine