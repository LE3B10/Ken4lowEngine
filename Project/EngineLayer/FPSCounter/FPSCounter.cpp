#define NOMINMAX
#include "FPSCounter.h"
#include <sstream>
#include <windows.h>

/// -------------------------------------------------------------
///					コンストラクタ
/// -------------------------------------------------------------
FPSCounter::FPSCounter(int targetFPS)
	: targetFPS_(targetFPS), frameCount_(0), currentFPS_(0.0f)
{
	reference_ = std::chrono::steady_clock::now();
	fpsReference_ = reference_;
}

/// -------------------------------------------------------------
///					フレーム開始時に呼ぶ
/// -------------------------------------------------------------
void FPSCounter::StartFrame()
{
	auto now = std::chrono::steady_clock::now();
	if (lastBegin_.time_since_epoch().count() != 0)
	{
		deltaSecond_ = std::chrono::duration<float>(now - lastBegin_).count();
	}
	else
	{
		deltaSecond_ = 1.0f / std::max(1, targetFPS_); // 初回は目標fpsから仮置き
	}
	lastBegin_ = now;
	reference_ = now; // 既存のフレーム基準も更新
}

/// -------------------------------------------------------------
///					フレーム終了時に呼ぶ（FPS固定＆計測）
/// -------------------------------------------------------------
void FPSCounter::EndFrame()
{
	frameCount_++;

	// ---- FPS計測（1秒ごとに更新）
	auto now = std::chrono::steady_clock::now();
	auto secElapsed = std::chrono::duration_cast<std::chrono::seconds>(now - fpsReference_);
	if (secElapsed.count() >= 1)
	{
		currentFPS_ = static_cast<float>(frameCount_) / static_cast<float>(secElapsed.count());
		frameCount_ = 0;
		fpsReference_ = now;

		std::ostringstream oss;
		oss << "Current FPS: " << currentFPS_ << " (Target: " << targetFPS_ << ")\n";
		OutputDebugStringA(oss.str().c_str());
	}

	// ---- FPS固定（steady_clock に統一して sleep_until）
	const int clampedTarget = std::max(1, targetFPS_);
	const auto frameDurFloat = std::chrono::duration<float>(1.0f / static_cast<float>(clampedTarget));
	const auto frameDur = std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameDurFloat);

	// このフレーム開始時刻 reference_ から、次フレームの締切（デッドライン）を決める
	auto deadline = reference_ + frameDur;
	now = std::chrono::steady_clock::now();

	if (now < deadline)
	{
		// 締切までスリープ（高精度に合わせてくれる）
		std::this_thread::sleep_until(deadline);
		reference_ = deadline;                 // 次フレームの基準はデッドライン
	}
	else
	{
		// ドリフト抑制のため、今を基準にする
		reference_ = now;
	}
}
