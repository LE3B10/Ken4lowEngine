#pragma once
#include "FPSCounter.h"

namespace Ken4lowEngine
{
	class GameTimer
	{
	public:
		static GameTimer* GetInstance();

		void Initialize(int targetFPS = 144);
		void Finalize();

		void BeginFrame();
		void EndFrame();

		// 区間計測
		void BeginUpdate();
		void EndUpdate();

		void BeginDraw();
		void EndDraw();

		void BeginPresent();
		void EndPresent();

	public:
		float GetDeltaTime() const { return fpsCounter_.GetDeltaTime(); }
		float GetFPS() const { return fpsCounter_.GetFPS(); }
		int GetTargetFPS() const { return targetFPS_; }

		float GetUpdateMs() const { return fpsCounter_.GetUpdateMs(); }
		float GetDrawMs() const { return fpsCounter_.GetDrawMs(); }
		float GetPresentMs() const { return fpsCounter_.GetPresentMs(); }
		float GetSleepMs() const { return fpsCounter_.GetSleepMs(); }
		float GetTotalFrameMs() const { return fpsCounter_.GetTotalFrameMs(); }

		void SetTargetFPS(int fps)
		{
			targetFPS_ = fps;
			fpsCounter_.SetTargetFPS(fps);
		}

	private:
		GameTimer() = default;
		~GameTimer() = default;
		GameTimer(const GameTimer&) = delete;
		GameTimer& operator=(const GameTimer&) = delete;

	private:
		FPSCounter fpsCounter_{ 144 };
		int targetFPS_ = 144;
		bool initialized_ = false;
	};
}