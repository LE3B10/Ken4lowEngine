#pragma once
#include <LevelData.h>
#include <memory>

/// -------------------------------------------------------------
/// 			　		レベルローダー
/// -------------------------------------------------------------
class LevelLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// レベルデータの読み込み
	static std::unique_ptr<LevelData> LoadLevel(const std::string& filePath);

private: /// ---------- メンバ変数 ---------- ///

	static inline const std::string fileDirectory_ = "Resources/JSON/"; // レベルデータのディレクトリパス
};

