#pragma once
#include <LevelData.h>
#include <memory>

/// -------------------------------------------------------------
///				　		レベルローダークラス
/// -------------------------------------------------------------
class LevelLoader
{
public: /// ---------- メンバ関数 ---------- ///

	// レベルデータの読み込み
	static std::unique_ptr<LevelData> LoadLevel(const std::string& filePath);
	
};

