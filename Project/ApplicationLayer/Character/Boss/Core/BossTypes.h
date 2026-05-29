#pragma once

/// ---------- ボス全体の状態 ---------- ///
enum class BossState
{
	Intro,				// 登場演出中
	Idle,				// 待機
	Move,				// 移動中
	Attack,				// 攻撃中
	Stagger,			// 軽いひるみ
	Down,				// 大きく崩れた状態
	PhaseTransition,	// フェーズ移行演出中
	Dead				// 死亡
};


/// ---------- ボスのフェーズ ---------- ///
enum class BossPhase
{
	Phase1, // フェーズ1
	Phase2,	// フェーズ2
	Phase3	// フェーズ3
};