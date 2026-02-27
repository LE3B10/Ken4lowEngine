#define NOMINMAX
#include "PlayerMotorComponent.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

bool PlayerMotorComponent::IsGrounded() const
{
	if (!tr_) return true;

	// groundQuery を使う場合だけ groundY_ 判定も有効
	const bool byQuery = (bool)groundQuery_
		&& (tr_->translate_.y <= groundY_ + groundSnapEpsilon_)
		&& (verticalVel_ <= 0.05f);

	return grounded_ || byQuery;
}

void PlayerMotorComponent::UpdateGroundFromQuery()
{
	if (!tr_ || !groundQuery_) return;

	float gy = groundY_;
	K4E::Vector3 n = groundNormal_;

	// 足元より少し上から下方向へ
	const K4E::Vector3 origin = tr_->translate_ + K4E::Vector3(0.0f, 1.0f, 0.0f);
	if (!groundQuery_(origin, groundProbeDistance_, gy, n)) return;

	groundY_ = gy;
	groundNormal_ = n;

	// 小段差は“上にスナップ”
	const float dy = groundY_ - tr_->translate_.y;
	if (dy > 0.0f && dy <= stepSnapHeight_ && verticalVel_ <= 0.0f)
	{
		tr_->translate_.y = groundY_;
		verticalVel_ = 0.0f;
	}
}

void PlayerMotorComponent::PreprocessInput(InputSnapshot& in, float dt)
{
	// まず地面更新（段差/坂の土台）
	UpdateGroundFromQuery();

	// dash cooldown
	dashCooldownTimer_ = std::max(0.0f, dashCooldownTimer_ - dt);

	// ===== クールタイム中はダッシュ入力（LCTRL）を無効化 =====
	// FSMがDash状態へ入ってしまうと「1フレームだけ足が止まる/走りが切れる」などの体感バグになりやすいので、
	// ここで入力自体を落として “何も起きない” 状態にする。
	if (dashCooldownTimer_ > 0.0f || dashTimer_ > 0.0f)
	{
		in.dashPressed = false;
	}

	// Jump Buffer
	if (in.jumpPressed) jumpBufferTimer_ = jumpBufferTime_;
	else jumpBufferTimer_ = std::max(0.0f, jumpBufferTimer_ - dt);

	// Coyote
	if (IsGrounded()) coyoteTimer_ = coyoteTime_;
	else coyoteTimer_ = std::max(0.0f, coyoteTimer_ - dt);

	// バッファが残っていて「いま跳べる」なら、このフレームの jumpPressed を強制発火
	if (jumpBufferTimer_ > 0.0f && (IsGrounded() || coyoteTimer_ > 0.0f))
	{
		in.jumpPressed = true;
		jumpBufferTimer_ = 0.0f;
	}
}

void PlayerMotorComponent::Jump()
{
	if (IsGrounded() || coyoteTimer_ > 0.0f)
	{
		verticalVel_ = jumpSpeed_;
		coyoteTimer_ = 0.0f;
		jumpBufferTimer_ = 0.0f;
	}
}

void PlayerMotorComponent::StartDash(float cameraYawRad, bool isAds)
{
	// ADS中はダッシュしない
	if (isAds) return;

	// クールダウン中は開始しない
	if (dashCooldownTimer_ > 0.0f) return;
	dashCooldownTimer_ = dashCooldown_;

	dashTimer_ = dashDuration_;

	const float s = -std::sinf(cameraYawRad);
	const float c = std::cosf(cameraYawRad);

	const float fwdX = s, fwdZ = c;
	const float rightX = c, rightZ = -s;

	float dx = rightX * moveX_ + fwdX * moveZ_;
	float dz = rightZ * moveX_ + fwdZ * moveZ_;

	const float lenSq = dx * dx + dz * dz;
	if (lenSq > 1e-6f)
	{
		const float invLen = 1.0f / std::sqrt(lenSq);
		dx *= invLen; dz *= invLen;
	}
	else
	{
		// 入力ゼロなら “カメラ前”
		dx = fwdX; dz = fwdZ;
	}

	dashDirX_ = dx;
	dashDirZ_ = dz;

	velX_ = dashDirX_ * dashSpeed_;
	velZ_ = dashDirZ_ * dashSpeed_;
}

void PlayerMotorComponent::Simulate(float dt, float cameraYawRad, LocoId locoId, bool isAds, bool isReloading)
{
	if (!tr_) return;

	grounded_ = false;

	// プレイヤーの位置を dt 秒だけ進めるときの処理。
	const Vector3 oldPosition = tr_->translate_;

	if (dashTimer_ > 0.0f)
	{
		dashTimer_ -= dt;

		// ★ダッシュ中は加速/減速を通さず固定速度
		velX_ = dashDirX_ * dashSpeed_;
		velZ_ = dashDirZ_ * dashSpeed_;

		tr_->translate_.x += velX_ * dt;
		tr_->translate_.z += velZ_ * dt;

		// 重力や接地スナップは従来通り（verticalVel_ だけ更新）
		return;
	}

	// 水平移動前にも地面更新（段差/坂）
	UpdateGroundFromQuery();

	// yawから “平面 forward/right”
	const float s = -std::sinf(cameraYawRad);
	const float c = std::cosf(cameraYawRad);
	const float fwdX = s, fwdZ = c;
	const float rightX = c, rightZ = -s;

	// 入力(moveX_/moveZ_)をワールド方向へ
	float moveDirX = rightX * moveX_ + fwdX * moveZ_;
	float moveDirZ = rightZ * moveX_ + fwdZ * moveZ_;

	const float lenSq = moveDirX * moveDirX + moveDirZ * moveDirZ;
	if (lenSq > 1e-6f)
	{
		const float invLen = 1.0f / std::sqrt(lenSq);
		moveDirX *= invLen;
		moveDirZ *= invLen;
	}
	else
	{
		moveDirX = 0.0f;
		moveDirZ = 0.0f;
	}

	auto approach = [](float cur, float target, float maxDelta)
		{
			if (cur < target) return std::min(cur + maxDelta, target);
			return std::max(cur - maxDelta, target);
		};

	const bool grounded = IsGrounded();

	// Dash
	if (locoId == LocoId::Dash && !isAds)
	{
		velX_ = dashDirX_ * dashSpeed_;
		velZ_ = dashDirZ_ * dashSpeed_;
		dashTimer_ = (dashTimer_ > dt) ? (dashTimer_ - dt) : 0.0f;
	}
	else
	{
		// ADS中にDash状態へ入ってしまった場合は即終了
		if (locoId == LocoId::Dash && isAds) { dashTimer_ = 0.0f; }

		float speed = 0.0f;
		// DashをADSで無効化した場合、速度計算は通常の歩き/走りとして扱う
		const LocoId speedId = (locoId == LocoId::Dash) ? (sprint_ ? LocoId::Run : LocoId::Walk) : locoId;
		switch (speedId)
		{
		case LocoId::Walk: speed = walkSpeed_; break;
		case LocoId::Run:  speed = runSpeed_;  break;
		case LocoId::Land: speed = (sprint_ ? runSpeed_ : walkSpeed_); break;
		case LocoId::Jump:
		case LocoId::Fall: speed = runSpeed_ * airControl_; break;
		default: speed = 0.0f; break;
		}

		// ADS中は移動速度を減速
		if (isAds) speed *= adsMoveMul_;

		// リロード中は移動速度を大幅減速
		if (isReloading) speed *= reloadMoveMul_;

		const float targetVX = moveDirX * speed;
		const float targetVZ = moveDirZ * speed;

		const float accel = grounded ? accelGround_ : accelAir_;
		const float decel = decelGround_;

		const float maxDX = ((std::abs(targetVX) > std::abs(velX_)) ? accel : decel) * dt;
		const float maxDZ = ((std::abs(targetVZ) > std::abs(velZ_)) ? accel : decel) * dt;

		velX_ = approach(velX_, targetVX, maxDX);
		velZ_ = approach(velZ_, targetVZ, maxDZ);

		tr_->translate_.x += velX_ * dt;
		tr_->translate_.z += velZ_ * dt;
	}

	// 水平後に地面更新（段差スナップ）
	UpdateGroundFromQuery();

	// 縦（重力）
	verticalVel_ -= gravity_ * dt;
	tr_->translate_.y += verticalVel_ * dt;

	/// ---------- 押し出し処理 ---------- ///
	if (worldAABBs_ && !worldAABBs_->empty())
	{
		float vy = verticalVel_;

		const auto result = WorldCollisionResolver::Resolve(
			*worldAABBs_,
			worldCollisionSettings_,
			oldPosition,
			tr_->translate_,
			true, // grounded判定を考慮
			&vy   // ジャンプ速度の修正を受け取る
		);

		// 衝突解決後の位置を適用
		verticalVel_ = vy;

		tr_->translate_ = result.fixedCenter + worldCollisionSettings_.centerOffset;
		grounded_ = result.grounded;

		if (result.grounded)
		{
			groundY_ = tr_->translate_.y;
		}
	}

	// ---- Debug speed ----
	if (!prevPosValid_)
	{
		prevPos_ = tr_->translate_;
		prevPosValid_ = true;
		dbgSpeedXZ_ = 0.0f;
		dbgSpeedY_ = 0.0f;
		return;
	}

	const K4E::Vector3 dp = tr_->translate_ - prevPos_;
	prevPos_ = tr_->translate_;
	dbgSpeedXZ_ = std::sqrt(dp.x * dp.x + dp.z * dp.z) / std::max(1e-6f, dt);
	dbgSpeedY_ = dp.y / std::max(1e-6f, dt);
}
