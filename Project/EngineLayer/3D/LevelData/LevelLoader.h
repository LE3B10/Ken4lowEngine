#pragma once
#include <LevelData.h>
#include <memory>

/// -------------------------------------------------------------
///				ã€€		ãƒ¬ãƒ™ãƒ«ãƒ­ãƒ¼ãƒ€ãƒ¼ã‚¯ãƒ©ã‚¹
/// -------------------------------------------------------------
class LevelLoader
{
public: /// ---------- ƒƒ“ƒoŠÖ” ---------- ///

	// ƒŒƒxƒ‹ƒf[ƒ^‚Ì“Ç‚İ‚İ
	static std::unique_ptr<LevelData> LoadLevel(const std::string& filePath);
	
};

