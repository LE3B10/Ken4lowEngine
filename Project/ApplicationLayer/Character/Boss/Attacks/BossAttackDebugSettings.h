#pragma once
#include <Vector3.h>

namespace K4E = Ken4lowEngine;

/// <summary>
/// ボス攻撃範囲をJSON保存しやすい単位で分離した設定。
/// </summary>
struct BossAttackSettings
{
	float attackRadius = 2.75f;
	float attackDistance = 2.80f;
	float attackHeight = 1.05f;
	float attackForwardOffset = 0.0f;
	float attackActiveStartTime = 0.30f;
	float attackActiveEndTime = 0.48f;
	int attackDamage = 20;
};

/// <summary>
/// Debug/ImGui専用の表示設定。
/// </summary>
struct BossDebugSettings
{
#ifdef _DEBUG
	bool showAttackRange = true;
#else
	bool showAttackRange = false;
#endif
};

/// <summary>
/// ボス攻撃の当たり具合をImGuiで確認するための実行時状態。
/// </summary>
struct BossDamageDebugState
{
	int bossAttackHitCount = 0;
	float lastPlayerDamage = 0.0f;
	float playerHp = 0.0f;
	bool isAttackActive = false;
	K4E::Vector3 attackCenter{};
	float distanceToPlayer = 0.0f;
};
