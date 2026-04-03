#pragma once
#include "FPSCounter.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                     時間管理クラス
	/// -------------------------------------------------------------
	class GameTimer
	{
	public:
		/// ---------------------------------------------------------
		///                シングルトン取得
		/// ---------------------------------------------------------
		static GameTimer* GetInstance();

		/// ---------------------------------------------------------
		///                初期化 / 更新
		/// ---------------------------------------------------------
		void Initialize(int targetFPS = 144);
		void Finalize();

		// フレーム開始
		void BeginFrame();

		// フレーム終了
		void EndFrame();

	public: /// ---------- Getter ---------- ///

		float GetDeltaTime() const { return fpsCounter_.GetDeltaTime(); }
		float GetFPS() const { return fpsCounter_.GetFPS(); }
		int GetTargetFPS() const { return targetFPS_; }

	public: /// ---------- Setter ---------- ///

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

} // namespace Ken4lowEngine