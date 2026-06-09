#pragma once
#include "GamePlayStageContext.h"

/// -------------------------------------------------------------
///                 ステージ目的の進行状態管理クラス
///
/// GamePlayWorldから使われ、StageRuleに基づくクリア/失敗条件と経過時間を集約する。
/// 実オブジェクトの当たり判定や破壊通知はWorld経由で受け取り、このクラスは状態判定だけを担当する。
/// -------------------------------------------------------------
class StageObjectiveManager
{
public: /// ---------- メンバ関数 ---------- ///

	// StageContextから現在ステージのルールと配置数を読み取り、進行カウンタを初期化する。
	void Initialize(const GamePlayStageContext& stageContext);
	// ステージ全体時間と防衛時間を進め、時間制限系の判定材料を更新する。
	void Update(float deltaTime);

	bool UsesWaveSystem() const { return stageRule_.useWaveSystem; }
	float GetStageElapsedSec() const { return stageElapsedSec_; }
	int GetActivatedDeviceCount() const { return activatedDeviceCount_; }
	int GetDevicePointCount() const { return devicePointCount_; }
	int GetDefenseTargetPointCount() const { return defenseTargetPointCount_; }
	int GetGoalPointCount() const { return goalPointCount_; }
	bool HasBossSpawnPoint() const { return hasBossSpawnPoint_; }
	bool HasReachedGoal() const { return reachedGoal_; }
	bool IsBossDefeated() const { return bossDefeated_; }
	bool IsDefenseTargetDestroyed() const { return defenseTargetDestroyed_; }
	bool IsStageObjectiveCleared(bool allWavesCleared) const;
	bool IsStageObjectiveFailed() const;

	// 装置接触イベントが実装されたら、各装置から呼ぶ進行加算APIとして使う。
	void AddActivatedDeviceCount(int amount = 1);
	// ゴール接触イベントが実装されたら、ゴール側から到達状態を通知する。
	void SetReachedGoal(bool reached);
	// ボス死亡またはクリアアイテム取得の結果を、目的条件へ反映する。
	void SetBossDefeated(bool defeated);
	// 防衛対象の破壊イベントが接続されたら、対象側から失敗状態を通知する。
	void SetDefenseTargetDestroyed(bool destroyed);

private: /// ---------- メンバ関数 ---------- ///

	int GetRequiredDeviceCount() const;

private: /// ---------- メンバ変数 ---------- ///

	GamePlayStageContext::StageRule stageRule_{};

	int devicePointCount_ = 0;
	int defenseTargetPointCount_ = 0;
	int goalPointCount_ = 0;
	bool hasBossSpawnPoint_ = false;

	int activatedDeviceCount_ = 0;
	float defendElapsedSec_ = 0.0f;
	float stageElapsedSec_ = 0.0f;

	bool reachedGoal_ = false;
	bool bossDefeated_ = false;
	bool defenseTargetDestroyed_ = false;
};
