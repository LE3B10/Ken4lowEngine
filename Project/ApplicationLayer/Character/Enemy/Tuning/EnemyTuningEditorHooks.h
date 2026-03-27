#pragma once
#include <functional>
#include "EnemyArchetype.h"

/// ------------------------------------------------------------
/// EnemyTuningEditorHooks
/// ------------------------------------------------------------
/// Enemy tuning editor から外へ通知するためのフック群。
/// 武器エディタの hooks と同じ発想で、
/// 「保存した」「削除した」「再読込した」後に何をしたいかを
/// エディタ本体から分離する。
/// ------------------------------------------------------------
struct EnemyTuningEditorHooks
{
	/// 保存後に呼ぶ
	/// 例:
	/// - 現在選択中の Enemy に再適用
	/// - スポナーへ通知
	std::function<void(EnemyArchetype)> onSaved;

	/// 削除後に呼ぶ
	std::function<void(EnemyArchetype)> onDeleted;

	/// Reload 後に呼ぶ
	std::function<void()> onReloaded;
};