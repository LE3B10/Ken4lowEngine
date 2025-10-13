#pragma once
#include <chrono>
#include <thread>


/// -------------------------------------------------------------
///						　FPS管理用クラス
/// -------------------------------------------------------------
class FPSCounter
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	FPSCounter(int targetFPS = 144);

	// フレーム開始時に呼ぶ
	void StartFrame();

	// フレーム終了時に呼ぶ（FPS固定＆計測）
	void EndFrame();

public: /// ---------- ゲッター ---------- ///

	// FPSの取得
	float GetFPS() const { return currentFPS_; } // 現在のFPSを取得

	float GetDeltaTime() const { return deltaSecond_; }

public: /// ---------- セッター ---------- ///

	// FPSの設定
	void SetTargetFPS(int fps) { targetFPS_ = fps; } // ターゲットFPSを変更

private: /// ---------- メンバ変数 ---------- ///

	// FPSの計算用
	std::chrono::steady_clock::time_point reference_;
	std::chrono::steady_clock::time_point fpsReference_;

	std::chrono::steady_clock::time_point lastBegin_{};
	float deltaSecond_ = 0.0f;

	int targetFPS_;
	int frameCount_;
	float currentFPS_;
};
