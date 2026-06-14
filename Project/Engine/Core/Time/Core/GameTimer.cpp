#include "GameTimer.h"

namespace Ken4lowEngine
{
	/// ----------------------------------------------
	///			シングルトンインスタンス取得
	/// ----------------------------------------------
	GameTimer* GameTimer::GetInstance()
	{
		// 関数内staticにすることで、初回呼び出し時に一度だけ生成される。
		static GameTimer instance;
		return &instance;
	}

	/// ----------------------------------------------
	///				 初期化処理
	/// ----------------------------------------------
	void GameTimer::Initialize(int targetFPS)
	{
		// 指定された目標FPSでFPSCounterをリセットし、計測開始できる状態にする。
		targetFPS_ = targetFPS;
		fpsCounter_.Reset(targetFPS_);
		initialized_ = true;
	}

	/// ----------------------------------------------
	///				 終了処理
	/// ----------------------------------------------
	void GameTimer::Finalize()
	{
		// 終了後に各Begin/End計測が動かないように初期化フラグを落とす。
		initialized_ = false;
	}

	/// ----------------------------------------------
	///				フレーム開始処理
	/// ----------------------------------------------
	void GameTimer::BeginFrame()
	{
		// 未初期化のまま呼ばれた場合でも動作できるように遅延初期化する。
		if (!initialized_)
		{
			Initialize(targetFPS_);
		}

		fpsCounter_.StartFrame();
	}

	/// ----------------------------------------------
	///				フレーム終了処理
	/// ----------------------------------------------
	void GameTimer::EndFrame()
	{
		// 初期化されていない場合は、FPS計算やログ出力を行わない。
		if (!initialized_)
		{
			return;
		}

		fpsCounter_.EndFrame();
	}

	/// ----------------------------------------------
	///		  Update/Draw/Present区間の計測
	/// -----------------------------------------------
	void GameTimer::BeginUpdate()
	{
		// Update区間の開始時刻を記録する。
		if (!initialized_) return;
		fpsCounter_.BeginUpdate();
	}

	void GameTimer::EndUpdate()
	{
		// Update区間の経過時間を確定する。
		if (!initialized_) return;
		fpsCounter_.EndUpdate();
	}

	void GameTimer::BeginDraw()
	{
		// Draw区間の開始時刻を記録する。
		if (!initialized_) return;
		fpsCounter_.BeginDraw();
	}

	void GameTimer::EndDraw()
	{
		// Draw区間の経過時間を確定する。
		if (!initialized_) return;
		fpsCounter_.EndDraw();
	}

	void GameTimer::BeginPresent()
	{
		// Present区間の開始時刻を記録する。
		if (!initialized_) return;
		fpsCounter_.BeginPresent();
	}

	void GameTimer::EndPresent()
	{
		// Present区間の経過時間を確定する。
		if (!initialized_) return;
		fpsCounter_.EndPresent();
	}
}
