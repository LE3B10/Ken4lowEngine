#include "GameTimer.h"

namespace Ken4lowEngine
{
	GameTimer* GameTimer::GetInstance()
	{
		static GameTimer instance;
		return &instance;
	}

	void GameTimer::Initialize(int targetFPS)
	{
		targetFPS_ = targetFPS;
		fpsCounter_.Reset(targetFPS_);
		initialized_ = true;
	}

	void GameTimer::Finalize()
	{
		initialized_ = false;
	}

	void GameTimer::BeginFrame()
	{
		if (!initialized_)
		{
			Initialize(targetFPS_);
		}
		fpsCounter_.StartFrame();
	}

	void GameTimer::EndFrame()
	{
		if (!initialized_)
		{
			return;
		}
		fpsCounter_.EndFrame();
	}

	void GameTimer::BeginUpdate()
	{
		if (!initialized_) return;
		fpsCounter_.BeginUpdate();
	}

	void GameTimer::EndUpdate()
	{
		if (!initialized_) return;
		fpsCounter_.EndUpdate();
	}

	void GameTimer::BeginDraw()
	{
		if (!initialized_) return;
		fpsCounter_.BeginDraw();
	}

	void GameTimer::EndDraw()
	{
		if (!initialized_) return;
		fpsCounter_.EndDraw();
	}

	void GameTimer::BeginPresent()
	{
		if (!initialized_) return;
		fpsCounter_.BeginPresent();
	}

	void GameTimer::EndPresent()
	{
		if (!initialized_) return;
		fpsCounter_.EndPresent();
	}
}