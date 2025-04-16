#include "FPSCounter.h"
#include <sstream>
#include <windows.h>

FPSCounter::FPSCounter(int targetFPS)
	: targetFPS_(targetFPS), frameCount_(0), currentFPS_(0.0f)
{
	reference_ = std::chrono::steady_clock::now();
	fpsReference_ = reference_;
}

void FPSCounter::StartFrame()
{
	reference_ = std::chrono::steady_clock::now();
}

void FPSCounter::EndFrame()
{
	frameCount_++;

	// FPS計測（1秒ごとに更新）
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - fpsReference_);
	if (elapsed.count() >= 1)
	{
		currentFPS_ = static_cast<float>(frameCount_) / elapsed.count();
		frameCount_ = 0;
		fpsReference_ = now;

		std::ostringstream oss;
		oss << "Current FPS: " << currentFPS_ << " (Target: " << targetFPS_ << ")\n";
		OutputDebugStringA(oss.str().c_str());
	}

	// FPS固定処理
	auto frameTime = std::chrono::microseconds(static_cast<int>(1000000.0f / targetFPS_));
	auto elapsedFrame = std::chrono::steady_clock::now() - reference_;

	if (elapsedFrame < frameTime)
	{
		auto sleepTime = frameTime - elapsedFrame;
		auto startSleep = std::chrono::high_resolution_clock::now();
		do {
			auto now = std::chrono::high_resolution_clock::now();
			auto elapsedSleep = now - startSleep;
			if (elapsedSleep >= sleepTime) break;
		} while (true);
		auto endSleep = std::chrono::steady_clock::now();

		// スリープの誤差を補正
		auto actualSleep = endSleep - startSleep;
		reference_ += (sleepTime - actualSleep);
	}
	else
	{
		reference_ = std::chrono::steady_clock::now();
	}
}