#pragma once
#include <vector>
#include <optional>
#include "IStageSelector.h"

/// -------------------------------------------------------------
///				ステージ情報リポジトリ（シングルトン）
/// -------------------------------------------------------------
class StageRepository
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス取得
	static StageRepository& GetInstance() { static StageRepository inst; return inst; }

	// ステージ情報リストの設定・取得
	void SetStages(std::vector<StageInfo> stages) { stages_ = std::move(stages); }
	const std::vector<StageInfo>& GetStages() const { return stages_; }

	// スタートインデックスの設定・取得
	void SetStartIndex(int idx) { startIndex_ = idx; }
	std::optional<int> GetStartIndex() const { return startIndex_; }
	void ClearStartIndex() { startIndex_.reset(); }

private: /// ---------- メンバ変数 ---------- ///

	StageRepository() = default;
	std::vector<StageInfo> stages_;
	std::optional<int> startIndex_;
};
