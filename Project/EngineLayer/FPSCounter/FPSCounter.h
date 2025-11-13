#pragma once
#include <chrono>
#include <thread>


/// -------------------------------------------------------------
///						　FPS管理用クラス
/// -------------------------------------------------------------
class FPSCounter
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// コンストラクタ。<br/>
	/// 目標FPSを指定して FPS 管理を開始します。
	/// </summary>
	/// <param name="targetFPS">
	/// 目標とする FPS 値。<br/>
	/// 例：60, 120, 144 など。1 未満を指定した場合は内部で 1 にクランプされます。
	/// </param>
	FPSCounter(int targetFPS = 144);

	/// <summary>
	/// フレーム開始時に呼び出します。<br/>
	/// ・前フレーム開始からの経過時間を計算し、deltaSecond_ に保存<br/>
	/// ・次回 EndFrame() でのスリープ基準時刻(reference_) を更新<br/>
	/// などの処理を行います。<br/>
	/// メインループの先頭で呼び出してください。
	/// </summary>
	void StartFrame();

	/// <summary>
	/// フレーム終了時に呼び出します（FPS固定＆計測）。<br/>
	/// ・1秒ごとに平均FPSを再計算し、currentFPS_ に保存（OutputDebugString でログ出力）<br/>
	/// ・targetFPS_ に基づいて 1フレームあたりの目標時間を算出し、<br/>
	/// 　不足分だけ std::this_thread::sleep_until でスリープ<br/>
	/// といった処理を行い、FPS を一定値付近に保ちます。<br/>
	/// メインループの末尾で呼び出してください。
	/// </summary>
	void EndFrame();

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 現在の FPS を取得します。<br/>
	/// 1 秒間に処理したフレーム数を平均した値で、EndFrame() 内で更新されます。
	/// </summary>
	/// <returns>直近 1 秒間の平均 FPS。</returns>
	float GetFPS() const { return currentFPS_; }

	/// <summary>
	/// 1 フレームの経過時間(秒)を取得します。<br/>
	/// StartFrame() 呼び出し時に前フレーム開始との差分として更新されます。<br/>
	/// ゲーム内の時間経過やアニメーションの補間に使用できます。
	/// </summary>
	/// <returns>前フレーム開始からの経過時間(秒)。</returns>
	float GetDeltaTime() const { return deltaSecond_; }

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// 目標とする FPS を変更します。<br/>
	/// 次回以降の EndFrame() から、ここで設定した FPS を基準にスリープ量を計算します。
	/// </summary>
	/// <param name="fps">新しく設定する目標 FPS 値。</param>
	void SetTargetFPS(int fps) { targetFPS_ = fps; }

private: /// ---------- メンバ変数 ---------- ///

	// FPSの計算用
	std::chrono::steady_clock::time_point reference_;    // フレーム時間計測基準
	std::chrono::steady_clock::time_point fpsReference_; // FPS計測基準

	// フレーム時間計測用
	std::chrono::steady_clock::time_point lastBegin_{}; // 前回のフレーム開始時刻

	// フレーム時間(秒)
	float deltaSecond_ = 0.0f;

	int targetFPS_;	   // 目標FPS
	int frameCount_;   // フレームカウント（FPS計測用）
	float currentFPS_; // 現在のFPS
};
