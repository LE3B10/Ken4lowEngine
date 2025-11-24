#pragma once

/// ---------- 敵キャラクター設定構造体 ---------- ///
struct EnemyConfig
{
	float maxHP;		  // 最大体力
	float moveSpeed;	  // 移動速度
	float attackRange;	  // 攻撃範囲
	float attackInterval; // 攻撃のインターバル（秒）
	float damage;		  // 攻撃力
	float viewDistance;	  // 視認距離
	float viewAngle;	  // 視認角度（度）

	// アニメーションとかエフェクト関連のパラメータもここに追加可能
};

// 将来敵にJson設定を読み込ませるときに使う予定