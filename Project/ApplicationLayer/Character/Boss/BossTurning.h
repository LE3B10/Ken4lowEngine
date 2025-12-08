#pragma once

/// ---------- チューニング構造体 ---------- ///
struct BossTurning
{
	// フェーズ構造体
	struct Phase
	{
		float rageHpThreshold = 0.5f; // このフェーズに入る体力割合の閾値（0.0～1.0）
	}phase;

	// 追尾構造体
	struct Chase
	{
		float walkSpeed = 4.0f;      // 追尾時の歩き速度
		float minDistance = 2.0f;    // これ以上は近づかない（プレイヤーとの最小距離）
		float rushDistance = 8.0f;   // この距離以上だと「遠い」
		float rushFarTime = 1.0f;    // 遠い状態が続いた時間で Rush 解禁
	}chase;

	// 突進構造体
	struct Rush
	{
		float duration = 2.0f;       // 突進全体時間
		float chargeTime = 0.5f;     // 突進溜め時間
		float speed = 250.0f;        // 突進最大速度
		float cooldown = 3.0f;		 // 突進終了後クールタイム
		float keepDistance = -10.0f; // プレイヤーを通り過ぎるようにする距離調整
	}rush;

	// 回転攻撃構造体
	struct Spin
	{
		float duration = 0.7f;		// 回転攻撃の時間
		float rotations = 3.0f;		// 回転数
		float cooldown = 0.5f;		// 終了後クールタイム
		float safeDistance = 2.0f;  // スピン開始時にこれくらいは距離を空けたい
		float hitRadius = 12.0f;	// スピン攻撃の“リーチ”（ボス中心からの半径）
		float damage = 10.0f;		// スピン一発のダメージ量
		float knockbackH = 0.6f;	// ノックバックの水平強さ
		float knockbackUp = 0.2f;	// ノックバックの縦方向
	} spin;
};