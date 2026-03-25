#include "PlayerFsmApi.h"

#include "Player.h"
#include "PlayerStateMachines.h"

/// -------------------------------------------------------------
///                    Player側 FSM 接着実装
/// -------------------------------------------------------------

void Player::FSM_StartReload()
{
	bool isReloading = false;
	float reloadTimer = 0.0f;
	float reloadSec = 0.0f;
	weapon_.GetReloadUI(isReloading, reloadTimer, reloadSec);

	// すでにリロード中なら二重開始しない
	if (!isReloading)
	{
		weapon_.StartReload();

		if (audio_.onReload)
		{
			audio_.onReload();
		}
	}
}

void Player::FSM_FireOnce()
{
	combat_.FireOnce(
		inputSnap_,
		refs_.bulletManager,
		refs_.collisionManager,
		[this](const K4E::Segment& seg)
		{
			SetSegment(seg);
		});
}

bool Player::FSM_IsMeleeFinished() const
{
	// 近接をまだ未実装なら true のままでOK
	return true;
}

void Player::FSM_StartMelee()
{
	// 近接をまだ未実装なら空でもOK
}

void Player::FSM_SetStunned(bool /*on*/)
{
	// スタン未実装なら空でOK
	// 後で damage_ / control lock / camera shake 等を繋ぐ
}

/// -------------------------------------------------------------
///                    PlayerAPI 実装
/// -------------------------------------------------------------

bool PlayerAPI::IsGrounded() const
{
	return player ? player->FSM_IsGrounded() : true;
}

float PlayerAPI::VerticalVelocity() const
{
	return player ? player->FSM_VerticalVelocity() : 0.0f;
}

void PlayerAPI::SetMoveInput(float x, float z)
{
	if (player) player->FSM_SetMoveInput(x, z);
}

void PlayerAPI::SetSprint(bool on)
{
	if (player) player->FSM_SetSprint(on);
}

void PlayerAPI::Jump()
{
	if (player) player->FSM_Jump();
}

void PlayerAPI::StartBlink()
{
	if (player) player->FSM_StartBlink();
}

bool PlayerAPI::CanStartBlink() const
{
	return player ? player->FSM_CanStartBlink() : false;
}

bool PlayerAPI::IsBlinkFinished() const
{
	return player ? player->FSM_IsBlinkFinished() : true;
}

bool PlayerAPI::CanFire() const
{
	return player ? player->FSM_CanFire() : false;
}

void PlayerAPI::FireOnce()
{
	if (player) player->FSM_FireOnce();
}

bool PlayerAPI::IsReloadFinished() const
{
	return player ? player->FSM_IsReloadFinished() : true;
}

void PlayerAPI::StartReload()
{
	if (player) player->FSM_StartReload();
}

bool PlayerAPI::IsMeleeFinished() const
{
	return player ? player->FSM_IsMeleeFinished() : true;
}

void PlayerAPI::StartMelee()
{
	if (player) player->FSM_StartMelee();
}

void PlayerAPI::SetAiming(bool on)
{
	if (player) player->FSM_SetAiming(on);
}

void PlayerAPI::SetStunned(bool on)
{
	if (player) player->FSM_SetStunned(on);
}