#pragma once
#include <cstdint>
#include "ApplicationLayer/DebugTools/StageChunk/StageChunkDebugController.h"

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class Input; }

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
class GamePlayFlow;
class GamePlayWorld;

/// -------------------------------------------------------------
///					ゲームプレイデバッグツール
/// -------------------------------------------------------------
class GamePlayDebugTools
{
public: /// ---------- メンバ関数 ---------- ///

	void Initialize() {}
	void Finalize() {}

	bool HandleFreezeToggle(K4E::Input* input, GamePlayFlow* flow);
	bool IsFrozen() const { return isImGuiFreeze_; }

	void UpdateFreeze();
	void UpdateDebugCamera(K4E::Input* input, GamePlayWorld* world);

	// world を受け取る
	void DrawImGui(GamePlayWorld* world);

	bool IsDebugCamera() const { return isDebugCamera_; }

private: /// ---------- メンバ関数 ---------- ///

	void EnterImGuiFreeze(K4E::Input* input, GamePlayFlow* flow);
	void ExitImGuiFreeze(K4E::Input* input);

private: /// ---------- メンバ変数 ---------- ///

	bool isDebugCamera_ = false;
	bool isImGuiFreeze_ = false;

	bool weaponEditorInitialized_ = false;
	int32_t lastAppliedWeaponID_ = 0;
	StageChunkDebugController stageChunkDebugController_{};
};