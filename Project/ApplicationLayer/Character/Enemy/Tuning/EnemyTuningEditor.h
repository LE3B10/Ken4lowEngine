#pragma once
#include "EnemyArchetype.h"
#include "EnemyTuningEditorHooks.h"

/// ------------------------------------------------------------
/// EnemyTuningEditor
/// ------------------------------------------------------------
/// Enemy tuning を ImGui から編集するエディタ。
/// - archetype 選択
/// - JSON 新規作成
/// - Load / Reload / Save / Delete
/// - パラメータ編集
/// を担当する。
/// ------------------------------------------------------------
class EnemyTuningEditor
{
public:
	void Initialize();
	void Draw(const EnemyTuningEditorHooks& hooks = {});

private:
	void EnsureInitialized();
	void LoadFromRepository();
	void SaveToRepository();
	void DrawArchetypeSelector();
	void DrawToolbar(const EnemyTuningEditorHooks& hooks);
	void DrawTuningFields();

private:
	bool initialized_ = false;

	/// 今 ImGui で編集している archetype
	EnemyArchetype selectedArchetype_ = EnemyArchetype::RifleGrunt;

	/// 編集用の作業バッファ
	EnemyTuning workingCopy_{};

	/// 変更フラグ
	bool dirty_ = false;

	/// 最後の操作結果表示用
	char statusText_[256] = "Enemy Tuning Editor Ready";
};