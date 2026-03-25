#pragma once

#include "PlayerStateMachines.h"

/// -------------------------------------------------------------
///  PlayerBrainComponent
///  - PlayerFSM（locomotion / combat / status）を実行する責務だけを持つ
///  - Player本体から「FSMを回す処理」を分離するためのコンポーネント
/// -------------------------------------------------------------
class PlayerBrainComponent
{
public:
	/// ---------------------------------------------------------
	/// 初期化
	/// - Player::Initialize から呼ぶ想定
	/// - 最初の状態遷移（Enter呼び出し）もここでまとめる
	/// ---------------------------------------------------------
	void Initialize(PlayerAPI& api)
	{
		api_ = &api;

		// ダミー入力で初期状態へ遷移
		InputSnapshot dummy{};
		PlayerContext ctx{ *api_, dummy, 0.0f };

		brain_.status.Change(ctx, StatusId::Normal);
		brain_.loco.Change(ctx, LocoId::Idle);
		brain_.combat.Change(ctx, CombatId::Hip);

		prevLocoId_ = brain_.loco.id;
	}

	/// ---------------------------------------------------------
	/// 毎フレーム更新
	/// - Player.cpp 側は「入力を作る」と「この関数を呼ぶ」だけにする
	/// ---------------------------------------------------------
	void Update(const InputSnapshot& input, float deltaTime)
	{
		if (!api_)
		{
			return;
		}

		PlayerContext ctx{ *api_, input, deltaTime };
		brain_.Update(ctx);
	}

	/// ---------------------------------------------------------
	/// アクセサ
	/// ---------------------------------------------------------
	PlayerBrain& GetBrain() { return brain_; }
	const PlayerBrain& GetBrain() const { return brain_; }

	LocoId GetPrevLocoId() const { return prevLocoId_; }
	void SetPrevLocoId(LocoId id) { prevLocoId_ = id; }

	LocoId GetCurrentLocoId() const { return brain_.loco.id; }
	CombatId GetCurrentCombatId() const { return brain_.combat.id; }
	StatusId GetCurrentStatusId() const { return brain_.status.id; }

private:
	/// PlayerAPI への参照
	PlayerAPI* api_ = nullptr;

	/// FSM本体
	PlayerBrain brain_{};

	/// 前フレームの locomotion 状態
	/// - FOV演出や「走行開始した瞬間」の判定に使いやすい
	LocoId prevLocoId_ = LocoId::Idle;
};