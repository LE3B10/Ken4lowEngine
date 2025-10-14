#pragma once

/// ---------- アイテムの定義 ---------- ///
enum class ItemType
{
	HealSmall,     // 小回復
	AmmoSmall,     // 弾薬少量
	ScoreBonus,    // スコア加算
	PowerUp,       // 一時的な強化（後で）
	ExperienceOrb, // 経験値オーブ（後で）
	Coin,          // コイン（後で）
	None           // 無効
};
