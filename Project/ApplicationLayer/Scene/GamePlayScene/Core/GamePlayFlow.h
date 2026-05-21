#pragma once
#include "PauseMenu.h"
#include "ResultMenu.h"

#include <functional>
#include <memory>

namespace Ken4lowEngine { class Input; }

/// ---------- 前方宣言 ---------- ///
class SceneManager;
class HUDManager;
class Player;

/// -------------------------------------------------------------
///				　	ゲームプレイのフロー制御
/// -------------------------------------------------------------
class GamePlayFlow
{
public: /// ---------- 列挙型 ---------- ///

	// ゲームの状態
	enum class State
	{
		Intro,		// ゲーム開始前のイントロ
		EquipIntro, // イントロ直後の武器構えアニメーション中
		Playing,	// 通常プレイ中
		GameClear,	// ゲームクリア
		GameOver,	// ゲームオーバー
	};

public: /// ---------- 構造体 ---------- ///

	// ポーズ中の更新に必要な情報をまとめた構造体
	struct PausedUpdateContext
	{
		float deltaTime;					  // デルタタイム
		Ken4lowEngine::Input* input;		  // ポーズ中でも入力は受け付けるため、Inputも渡す
		HUDManager* hud = nullptr;			  // ポーズ中でもHUDの状態を見たいことがあるため、HUDManagerも渡す
		Player* player = nullptr;			  // ポーズ中でもプレイヤーの状態を見たいことがあるため、Playerも渡す
		SceneManager* sceneManager = nullptr; // ポーズ中の更新でシーン遷移する可能性があるため、SceneManagerも渡す
		bool lockCursorOnResume = true;		  // ポーズ解除時にカーソルをロックするかどうか
	};

	struct ResultUpdateContext
	{
		float deltaTime;					  // デルタタイム
		Ken4lowEngine::Input* input;		  // 結果画面でも入力は受け付けるため、Inputも渡す
		SceneManager* sceneManager = nullptr; // 結果画面の更新でシーン遷移する可能性があるため、SceneManagerも渡す
		std::function<void()> onRetry;		  // リトライボタンが押されたときのコールバック
		std::function<void()> onNextStage;	  // 次のステージへ進むボタンが押されたときのコールバック
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 終了処理
	void Finalize();

	// 新しいゲームの開始に向けて状態をリセットする
	void ResetForNewGame(bool startIntro);

	// ゲームプレイ開始
	void StartPlaying();

	// ポーズ開始
	void EnterPause(Ken4lowEngine::Input* input);

	// ポーズ解除
	void ExitPause(Ken4lowEngine::Input* input, bool lockCursorOnResume);

	// ポーズ解除
	void CancelPause();

	// ゲームクリア開始
	void EnterGameClear(Ken4lowEngine::Input* input, const std::function<void()>& onUnlockNextStage);

	// ゲームオーバー開始
	void EnterGameOver(Ken4lowEngine::Input* input);

	// ポーズ中の更新
	void UpdatePaused(const PausedUpdateContext& ctx);

	// 結果画面の更新
	void UpdateResult(const ResultUpdateContext& ctx);

	// UIの描画
	void DrawUI();

public: /// ---------- アクセサ ---------- ///

	// ゲームが一時停止しているかどうか
	bool IsPaused() const { return isPaused_; }

	// イントロを再生中かどうか
	bool IsIntro() const { return state_ == State::Intro; }
	bool IsEquipIntro() const { return state_ == State::EquipIntro; }

	// 結果画面（ゲームクリア or ゲームオーバー）にいるかどうか
	bool IsResultState() const { return state_ == State::GameClear || state_ == State::GameOver; }

	// ゲーム状態を取得する
	State GetState() const { return state_; }

	// ゲーム状態を直接設定する
	void SetState(State s) { state_ = s; }

private: /// ---------- メンバ変数 ---------- ///

	// ポーズメニュー
	std::unique_ptr<PauseMenu> pauseMenu_;

	// 結果画面
	std::unique_ptr<ResultMenu> resultMenu_;

	// ゲームが一時停止しているかどうか
	bool isPaused_ = false;

	// 結果画面で入力を受け付けるまでのクールタイム
	float resultInputCooldown_ = 0.0f;

	// 現在のゲーム状態
	State state_ = State::Playing;
};
