#define NOMINMAX
#include "FPSCounter.h"
#include <sstream>
#include <windows.h>
#include <algorithm>

namespace Ken4lowEngine
{
	FPSCounter::FPSCounter(int targetFPS)
		: targetFPS_(targetFPS)
	{
		auto now = Clock::now();
		frameBegin_ = now;
		lastBegin_ = now;
		fpsReference_ = now;
	}

	void FPSCounter::StartFrame()
	{
		const auto now = Clock::now();

		if (lastBegin_.time_since_epoch().count() != 0)
		{
			deltaSecond_ = std::chrono::duration<float>(now - lastBegin_).count();
		}
		else
		{
			deltaSecond_ = 1.0f / static_cast<float>(std::max(1, targetFPS_));
		}

		lastBegin_ = now;
		frameBegin_ = now;

		updateMs_ = 0.0f;
		drawMs_ = 0.0f;
		presentMs_ = 0.0f;
		sleepMs_ = 0.0f;
		totalFrameMs_ = 0.0f;
	}

	void FPSCounter::BeginUpdate()
	{
		updateBegin_ = Clock::now();
	}

	void FPSCounter::EndUpdate()
	{
		updateMs_ = ToMs(Clock::now() - updateBegin_);
	}

	void FPSCounter::BeginDraw()
	{
		drawBegin_ = Clock::now();
	}

	void FPSCounter::EndDraw()
	{
		drawMs_ = ToMs(Clock::now() - drawBegin_);
	}

	void FPSCounter::BeginPresent()
	{
		presentBegin_ = Clock::now();
	}

	void FPSCounter::EndPresent()
	{
		presentMs_ = ToMs(Clock::now() - presentBegin_);
	}

	void FPSCounter::EndFrame()
	{
		const auto frameEnd = Clock::now();
		totalFrameMs_ = ToMs(frameEnd - frameBegin_);

		++frameCount_;
		const float elapsedSec = std::chrono::duration<float>(frameEnd - fpsReference_).count();

		sleepMs_ = 0.0f; // 一旦FPS固定を切る

		if (elapsedSec >= 1.0f)
		{
			currentFPS_ = static_cast<float>(frameCount_) / elapsedSec;
			frameCount_ = 0;
			fpsReference_ = frameEnd;

			std::ostringstream oss;
			oss
				<< "FPS: " << currentFPS_
				<< " / Target: " << targetFPS_
				<< " | Update: " << updateMs_ << " ms"
				<< " | Draw: " << drawMs_ << " ms"
				<< " | Present: " << presentMs_ << " ms"
				<< " | Sleep: " << sleepMs_ << " ms"
				<< " | Total: " << totalFrameMs_ << " ms\n";

			OutputDebugStringA(oss.str().c_str());
		}
	}

	void FPSCounter::Reset(int targetFPS)
	{
		targetFPS_ = targetFPS;
		frameCount_ = 0;
		currentFPS_ = 0.0f;
		deltaSecond_ = 0.0f;

		updateMs_ = 0.0f;
		drawMs_ = 0.0f;
		presentMs_ = 0.0f;
		sleepMs_ = 0.0f;
		totalFrameMs_ = 0.0f;

		const auto now = Clock::now();
		frameBegin_ = now;
		lastBegin_ = now;
		fpsReference_ = now;

		updateBegin_ = {};
		drawBegin_ = {};
		presentBegin_ = {};
	}
}