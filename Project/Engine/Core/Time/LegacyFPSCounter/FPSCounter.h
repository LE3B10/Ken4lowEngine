#pragma once
#include <chrono>
#include <thread>

namespace Ken4lowEngine
{
	class FPSCounter
	{
	public:
		FPSCounter(int targetFPS = 144);

		// フレーム先頭
		void StartFrame();

		// 各区間の計測
		void BeginUpdate();
		void EndUpdate();

		void BeginDraw();
		void EndDraw();

		void BeginPresent();
		void EndPresent();

		// フレーム末尾（FPS制御＆ログ出力）
		void EndFrame();

		void Reset(int targetFPS = 144);

	public: /// ---------- Getter ---------- ///

		float GetFPS() const { return currentFPS_; }
		float GetDeltaTime() const { return deltaSecond_; }

		float GetUpdateMs() const { return updateMs_; }
		float GetDrawMs() const { return drawMs_; }
		float GetPresentMs() const { return presentMs_; }
		float GetSleepMs() const { return sleepMs_; }
		float GetTotalFrameMs() const { return totalFrameMs_; }

	public: /// ---------- Setter ---------- ///

		void SetTargetFPS(int fps) { targetFPS_ = fps; }

	private:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		float ToMs(const Clock::duration& d) const
		{
			return std::chrono::duration<float, std::milli>(d).count();
		}

	private:
		TimePoint frameBegin_{};
		TimePoint lastBegin_{};
		TimePoint fpsReference_{};

		TimePoint updateBegin_{};
		TimePoint drawBegin_{};
		TimePoint presentBegin_{};

		float deltaSecond_ = 0.0f;

		float updateMs_ = 0.0f;
		float drawMs_ = 0.0f;
		float presentMs_ = 0.0f;
		float sleepMs_ = 0.0f;
		float totalFrameMs_ = 0.0f;

		int targetFPS_ = 144;
		int frameCount_ = 0;
		float currentFPS_ = 0.0f;
	};
}