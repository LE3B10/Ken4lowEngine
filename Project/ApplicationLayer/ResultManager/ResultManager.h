#pragma once
#include <Sprite.h>
#include "NumberSpriteDrawer.h"

#include <memory>
#include <string>

class ResultManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

public: /// ---------- セッター ---------- ///

	// 最終スコアの設定
	void SetFinalScore(int score) { finalScore_ = score; }

	// キル数の設定
	void SetKillCount(int kills) { killCount_ = kills; }

	// ウェーブ数の設定
	void SetWaveCount(int wave) { waveCount_ = wave; }

public: /// ---------- ゲッター ---------- ///

	// ウェーブを取得
	int GetWaveCount() const { return waveCount_; }

	// 結果画面の表示をリクエスト
	bool IsRestartRequested() const { return restartRequested_; }

	// タイトルへ戻るの表示をリクエスト
	bool IsReturnToTitleRequested() const { return returnToTitleRequested_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Sprite> resultBackground_; // 結果画面の背景
	std::unique_ptr<Sprite> restartText_;	   // リスタートテキスト
	std::unique_ptr<Sprite> titleText_;		   // タイトルへ戻るテキスト

	std::unique_ptr<NumberSpriteDrawer> scoreDrawer_; // スコア描画用
	std::unique_ptr<NumberSpriteDrawer> killDrawer_;  // キル数描画用
	std::unique_ptr<NumberSpriteDrawer> waveDrawer_;  // ウェーブ数描画用

	int finalScore_ = 0; // 最終スコア
	int killCount_ = 0;  // キル数
	int waveCount_ = 1;  // ウェーブ数

	bool restartRequested_ = false;		  // リスタートリクエストフラグ
	bool returnToTitleRequested_ = false; // タイトルへ戻るリクエストフラグ
};

