#pragma once
#include <variant>
#include <optional>
#include <cstdint>

/// ===== 入力スナップショット（毎フレームPlayerが作る）=====
#include "PlayerInputSnapshot.h"

/// ---------- 前方宣言 ---------- ///
class Player;

/// ===== Player側に用意してほしい最低限の“問い合わせ/命令”IF（例）=====
/// ここはあなたのPlayer/CharacterMotor/Weaponへ繋ぐ
struct PlayerAPI
{
	Player* player = nullptr; // Playerへの紐づけ

	// --- locomotion queries ---
	bool IsGrounded() const;        // 接地判定
	float VerticalVelocity() const; // 上下速度

	// --- locomotion commands ---
	void SetMoveInput(float x, float z);
	void SetSprint(bool on);
	void Jump();                    // ジャンプ開始
	void StartDash();               // ダッシュ開始
	bool IsDashFinished() const;

	// --- combat queries/commands ---
	bool CanFire() const;
	void FireOnce();                // 1発撃つ（Autoなら連射はステート側で繰返し）
	bool IsReloadFinished() const;
	void StartReload();
	bool IsMeleeFinished() const;
	void StartMelee();

	// --- aim ---
	void SetAiming(bool on);

	// --- stun ---
	void SetStunned(bool on);
};

struct PlayerContext
{
	PlayerAPI& api;
	const InputSnapshot& in;
	float dt = 0.0f;
};

/// ===== 小さいヘルパ：optional遷移 =====
template<class Id>
using Next = std::optional<Id>;

/// =======================================================
///  Locomotion FSM
/// =======================================================
enum class LocoId : uint8_t { Idle, Walk, Run, Jump, Fall, Land, Dash };

/// --------------------------------------------------------
///					　　待機状態
/// --------------------------------------------------------
struct LocoIdle
{
	void Enter(PlayerContext& ctx) { ctx.api.SetSprint(false); }
	Next<LocoId> Update(PlayerContext& ctx) {
		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ);

		if (!ctx.api.IsGrounded()) return LocoId::Fall;
		if (ctx.in.dashPressed)    return LocoId::Dash;
		if (ctx.in.jumpPressed)    return LocoId::Jump;

		const bool moving = (ctx.in.moveX * ctx.in.moveX + ctx.in.moveZ * ctx.in.moveZ) > 0.0001f;
		if (moving) return ctx.in.sprintHeld ? LocoId::Run : LocoId::Walk;

		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///					　　歩行状態
/// --------------------------------------------------------
struct LocoWalk
{
	void Enter(PlayerContext&) {}
	Next<LocoId> Update(PlayerContext& ctx) {
		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ);
		ctx.api.SetSprint(false);

		if (!ctx.api.IsGrounded()) return LocoId::Fall;
		if (ctx.in.dashPressed)    return LocoId::Dash;
		if (ctx.in.jumpPressed)    return LocoId::Jump;

		const bool moving = (ctx.in.moveX * ctx.in.moveX + ctx.in.moveZ * ctx.in.moveZ) > 0.0001f;
		if (!moving) return LocoId::Idle;
		if (ctx.in.sprintHeld) return LocoId::Run;

		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///					　　走り状態
/// --------------------------------------------------------
struct LocoRun
{
	void Enter(PlayerContext&) {}
	Next<LocoId> Update(PlayerContext& ctx)
	{
		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ);
		ctx.api.SetSprint(true);

		if (!ctx.api.IsGrounded()) return LocoId::Fall;
		if (ctx.in.dashPressed)    return LocoId::Dash;
		if (ctx.in.jumpPressed)    return LocoId::Jump;

		const bool moving = (ctx.in.moveX * ctx.in.moveX + ctx.in.moveZ * ctx.in.moveZ) > 0.0001f;
		if (!moving) return LocoId::Idle;
		if (!ctx.in.sprintHeld) return LocoId::Walk;

		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///						  空中状態
/// --------------------------------------------------------
struct LocoJump
{
	void Enter(PlayerContext& ctx) { ctx.api.Jump(); }
	Next<LocoId> Update(PlayerContext& ctx)
	{
		// 上昇が終わったら落下へ
		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ); // 空中でも入力反映
		ctx.api.SetSprint(ctx.in.sprintHeld); // 空中でもスプリント状態を維持
		if (ctx.api.VerticalVelocity() <= 0.0f) return LocoId::Fall;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///					　	  落下状態
/// --------------------------------------------------------
struct LocoFall
{
	void Enter(PlayerContext&) {}
	Next<LocoId> Update(PlayerContext& ctx)
	{
		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ); // 空中でも入力反映
		ctx.api.SetSprint(ctx.in.sprintHeld); // 空中でもスプリント状態を維持
		if (ctx.api.IsGrounded()) return LocoId::Land;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///						着地状態
/// --------------------------------------------------------
struct LocoLand
{
	float t = 0.0f; // 着地硬直(例)
	void Enter(PlayerContext&) { t = 0.0f; }
	Next<LocoId> Update(PlayerContext& ctx)
	{
		// 着地硬直処理（例）
		t += ctx.dt;

		ctx.api.SetMoveInput(ctx.in.moveX, ctx.in.moveZ); // 着地中でも入力反映
		ctx.api.SetSprint(ctx.in.sprintHeld); // 着地中でもスプリント状態を維持

		if (t < 0.0f) return std::nullopt; // 例: 80msだけ硬直

		const bool moving = (ctx.in.moveX * ctx.in.moveX + ctx.in.moveZ * ctx.in.moveZ) > 0.0001f;
		if (!moving) return LocoId::Idle;
		return ctx.in.sprintHeld ? LocoId::Run : LocoId::Walk;
	}
	void Exit(PlayerContext&) {}
};

/// --------------------------------------------------------
///						ダッシュ状態
/// --------------------------------------------------------
struct LocoDash
{
	void Enter(PlayerContext& ctx) { ctx.api.StartDash(); }
	Next<LocoId> Update(PlayerContext& ctx) {
		// ダッシュ中は入力を無視したいならここで固定
		if (!ctx.api.IsGrounded()) return LocoId::Fall;
		if (ctx.api.IsDashFinished()) {
			const bool moving = (ctx.in.moveX * ctx.in.moveX + ctx.in.moveZ * ctx.in.moveZ) > 0.0001f;
			if (!moving) return LocoId::Idle;
			return ctx.in.sprintHeld ? LocoId::Run : LocoId::Walk;
		}
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

using LocoState = std::variant<LocoIdle, LocoWalk, LocoRun, LocoJump, LocoFall, LocoLand, LocoDash>;

/// =======================================================
///  Combat FSM
/// =======================================================
enum class CombatId : uint8_t { Hip, Aim, Shoot, Reload, Melee };

struct CombatHip
{
	void Enter(PlayerContext& ctx) { ctx.api.SetAiming(false); }
	Next<CombatId> Update(PlayerContext& ctx) {
		if (ctx.in.meleePressed)  return CombatId::Melee;
		if (ctx.in.reloadPressed) return CombatId::Reload;
		if (ctx.in.aimHeld)       return CombatId::Aim;
		if (ctx.in.fireHeld)      return CombatId::Shoot;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

struct CombatAim {
	void Enter(PlayerContext& ctx) { ctx.api.SetAiming(true); }
	Next<CombatId> Update(PlayerContext& ctx) {
		if (ctx.in.meleePressed)  return CombatId::Melee;
		if (ctx.in.reloadPressed) return CombatId::Reload;
		if (!ctx.in.aimHeld)      return CombatId::Hip;
		if (ctx.in.fireHeld)      return CombatId::Shoot;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

struct CombatShoot {
	float fireTimer = 0.0f; // 例: 連射管理をここでやる（武器側に寄せてもOK）
	void Enter(PlayerContext& ctx) {
		fireTimer = 0.0f;
		if (ctx.api.CanFire()) ctx.api.FireOnce();
	}
	Next<CombatId> Update(PlayerContext& ctx) {
		// トリガー離したら戻る
		if (!ctx.in.fireHeld) return ctx.in.aimHeld ? CombatId::Aim : CombatId::Hip;

		// 連射するならここでクールダウンを進めて再発射（例）
		fireTimer += ctx.dt;
		// if (fireTimer >= weapon.fireInterval && ctx.api.CanFire()) { ctx.api.FireOnce(); fireTimer = 0; }

		// 弾切れ等で撃てないなら戻す（必要なら）
		// if (!ctx.api.CanFire()) return ctx.in.aimHeld ? CombatId::Aim : CombatId::Hip;

		// リロード割り込みを許可するなら
		if (ctx.in.reloadPressed) return CombatId::Reload;

		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

struct CombatReload {
	void Enter(PlayerContext& ctx) { ctx.api.StartReload(); }
	Next<CombatId> Update(PlayerContext& ctx) {
		if (ctx.api.IsReloadFinished()) return ctx.in.aimHeld ? CombatId::Aim : CombatId::Hip;
		// ここで“キャンセル許可”するなら条件追加
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

struct CombatMelee {
	void Enter(PlayerContext& ctx) { ctx.api.StartMelee(); }
	Next<CombatId> Update(PlayerContext& ctx) {
		if (ctx.api.IsMeleeFinished()) return ctx.in.aimHeld ? CombatId::Aim : CombatId::Hip;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

using CombatState = std::variant<CombatHip, CombatAim, CombatShoot, CombatReload, CombatMelee>;

/// =======================================================
///  Status FSM（Stunnedが最優先で上書き）
/// =======================================================
enum class StatusId : uint8_t { Normal, Stunned };

struct StatusNormal {
	void Enter(PlayerContext& ctx) { ctx.api.SetStunned(false); }
	Next<StatusId> Update(PlayerContext&) { return std::nullopt; }
	void Exit(PlayerContext&) {}
};

struct StatusStunned {
	float remaining = 0.0f;
	explicit StatusStunned(float sec = 0.0f) : remaining(sec) {}
	void Enter(PlayerContext& ctx) { ctx.api.SetStunned(true); }
	Next<StatusId> Update(PlayerContext& ctx) {
		remaining -= ctx.dt;
		if (remaining <= 0.0f) return StatusId::Normal;
		return std::nullopt;
	}
	void Exit(PlayerContext&) {}
};

using StatusState = std::variant<StatusNormal, StatusStunned>;

/// =======================================================
///  それぞれのFSM本体（共通テンプレを作ってもOKだけど、まずは直書き）
/// =======================================================
struct LocomotionFSM {
	LocoId id = LocoId::Idle;
	LocoState st = LocoIdle{};

	void Change(PlayerContext& ctx, LocoId next) {
		std::visit([&](auto& s) { s.Exit(ctx); }, st);
		id = next;
		switch (id) {
		case LocoId::Idle: st = LocoIdle{}; break;
		case LocoId::Walk: st = LocoWalk{}; break;
		case LocoId::Run:  st = LocoRun{};  break;
		case LocoId::Jump: st = LocoJump{}; break;
		case LocoId::Fall: st = LocoFall{}; break;
		case LocoId::Land: st = LocoLand{}; break;
		case LocoId::Dash: st = LocoDash{}; break;
		}
		std::visit([&](auto& s) { s.Enter(ctx); }, st);
	}

	void Update(PlayerContext& ctx) {
		if (auto next = std::visit([&](auto& s) { return s.Update(ctx); }, st)) {
			Change(ctx, *next);
		}
	}
};

struct CombatFSM {
	CombatId id = CombatId::Hip;
	CombatState st = CombatHip{};

	void Change(PlayerContext& ctx, CombatId next) {
		std::visit([&](auto& s) { s.Exit(ctx); }, st);
		id = next;
		switch (id) {
		case CombatId::Hip:    st = CombatHip{};    break;
		case CombatId::Aim:    st = CombatAim{};    break;
		case CombatId::Shoot:  st = CombatShoot{};  break;
		case CombatId::Reload: st = CombatReload{}; break;
		case CombatId::Melee:  st = CombatMelee{};  break;
		}
		std::visit([&](auto& s) { s.Enter(ctx); }, st);
	}

	void Update(PlayerContext& ctx) {
		if (auto next = std::visit([&](auto& s) { return s.Update(ctx); }, st)) {
			Change(ctx, *next);
		}
	}
};

struct StatusFSM {
	StatusId id = StatusId::Normal;
	StatusState st = StatusNormal{};

	void Change(PlayerContext& ctx, StatusId next, float stunSec = 0.0f) {
		std::visit([&](auto& s) { s.Exit(ctx); }, st);
		id = next;
		switch (id) {
		case StatusId::Normal:  st = StatusNormal{}; break;
		case StatusId::Stunned: st = StatusStunned{ stunSec }; break;
		}
		std::visit([&](auto& s) { s.Enter(ctx); }, st);
	}

	void Update(PlayerContext& ctx) {
		if (auto next = std::visit([&](auto& s) { return s.Update(ctx); }, st)) {
			Change(ctx, *next);
		}
	}

	void RequestStun(PlayerContext& ctx, float sec) {
		Change(ctx, StatusId::Stunned, sec);
	}

	bool IsStunned() const { return id == StatusId::Stunned; }
};

/// =======================================================
///  最終まとめ：Playerが持つ “3FSM”
/// =======================================================
struct PlayerBrain
{
	StatusFSM status;
	LocomotionFSM loco;
	CombatFSM combat;

	void Update(PlayerContext& ctx) {
		status.Update(ctx);
		if (status.IsStunned()) {
			// スタン中は移動/戦闘を止める（必要ならノックバック処理だけする）
			return;
		}
		loco.Update(ctx);
		combat.Update(ctx);
	}
};