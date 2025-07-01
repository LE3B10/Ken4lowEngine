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

	void SetFinalScore(int score) { finalScore_ = score; }
	void SetKillCount(int kills) { killCount_ = kills; }
	void SetWaveCount(int wave) { waveCount_ = wave; }

	// ウェーブを取得
	int GetWaveCount() const { return waveCount_; }

	// 結果画面の表示をリクエスト
	bool IsRestartRequested() const { return restartRequested_; }
	bool IsReturnToTitleRequested() const { return returnToTitleRequested_; }

private: /// ---------- メンバ変数 ---------- ///

	std::unique_ptr<Sprite> resultBackground_;
	std::unique_ptr<Sprite> restartText_;
	std::unique_ptr<Sprite> titleText_;

	std::unique_ptr<NumberSpriteDrawer> scoreDrawer_;
	std::unique_ptr<NumberSpriteDrawer> killDrawer_;
	std::unique_ptr<NumberSpriteDrawer> waveDrawer_;

	int finalScore_ = 0;
	int killCount_ = 0;
	int waveCount_ = 1;

	bool restartRequested_ = false;
	bool returnToTitleRequested_ = false;
};

