#define NOMINMAX
#include "PlayerMotorComponent.h"

#include <algorithm>
#include <cmath>

using namespace Ken4lowEngine;

bool PlayerMotorComponent::IsGrounded() const
{
	if (!tr_) return true;
	if (IsOutsideStage()) return false;

	// groundQuery を使う場合だけ groundY_ 判定も有効
	const bool byQuery = (bool)groundQuery_
		&& (tr_->translate_.y <= groundY_ + groundSnapEpsilon_)
		&& (verticalVel_ <= 0.05f);

	// Physics接地は有効時だけOR追加し、falseでも既存の接地判定を打ち消さない。
	return grounded_ || byQuery || (usePhysicsGrounded_ && physicsGrounded_);
}

bool PlayerMotorComponent::IsInsideStageXZ(const K4E::Vector3& position) const
{
	// Stage情報が未接続の場面では従来挙動を維持し、範囲外として扱わない。
	if (!worldAABBs_ || worldAABBs_->empty())
	{
		return true;
	}

	const float margin = std::max(stageBoundsMargin_, 0.0f);
	for (const K4E::AABB& stageAABB : *worldAABBs_)
	{
		// Player足元のXZ点がどれか一つのStage領域内なら、床端の余白を含めてStage上とみなす。
		if (position.x >= stageAABB.min.x - margin && position.x <= stageAABB.max.x + margin &&
			position.z >= stageAABB.min.z - margin && position.z <= stageAABB.max.z + margin)
		{
			return true;
		}
	}

	return false;
}

bool PlayerMotorComponent::IsOutsideStage() const
{
	// Transform未接続時は判定不能なので、既存処理を止めないようStage内として扱う。
	return tr_ && !IsInsideStageXZ(tr_->translate_);
}

void PlayerMotorComponent::SetUsePhysicsGrounded(bool enabled)
{
	// OFF時はIsGroundedの結果が完全に既存経路だけで決まるよう、使用フラグのみ切り替える。
	usePhysicsGrounded_ = enabled;
}

void PlayerMotorComponent::SetPhysicsGrounded(bool grounded)
{
	// PhysicsWorldの評価値を保持し、使用可否とは分離してDebug比較できるようにする。
	physicsGrounded_ = grounded;
}

bool PlayerMotorComponent::IsUsingPhysicsGrounded() const
{
	// FSMがPhysics接地を併用中かDebug表示から確認できるようにする。
	return usePhysicsGrounded_;
}

void PlayerMotorComponent::UpdateGroundFromQuery()
{
	// Stage外では過去のgroundYへスナップせず、重力積分による落下を優先する。
	if (!tr_ || !groundQuery_ || IsOutsideStage()) return;

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

	// blink cooldown
	blinkCooldownTimer_ = std::max(0.0f, blinkCooldownTimer_ - dt);

	// Ladder内ではShift/Ctrlを下降入力として保持し、Blink開始入力には渡さない。
	ladderDescendHeld_ = isInLadderArea_ && (in.sprintHeld || in.blinkPressed);
	if (isInLadderArea_)
	{
		in.blinkPressed = false;
		blinkTimer_ = 0.0f;
	}

	if (isInLadderArea_ && in.jumpPressed)
	{
		// Space押下は梯子を再取得しないロック付き離脱ジャンプとして、通常重力へ即座に戻す。
		isInLadderArea_ = false;
		isClimbingLadder_ = false;
		ladderDetachLocked_ = true;
		verticalVel_ = jumpSpeed_;
		grounded_ = false;
		in.jumpPressed = false;
		in.jumpHeld = false;
	}

	// クールタイム中はダッシュ入力を無効化
	if (blinkCooldownTimer_ > 0.0f || blinkTimer_ > 0.0f)
	{
		in.blinkPressed = false;
	}

	// ------------------------------------------------------------
	// Jump Buffer
	// 押した瞬間でも、押しっぱなしでも、少し先のジャンプを予約できるようにする
	// ------------------------------------------------------------
	if (in.jumpPressed)
	{
		jumpBufferTimer_ = jumpBufferTime_;
	}
	else if (in.jumpHeld && IsGrounded())
	{
		// マイクラ風：押しっぱなしなら着地中に次ジャンプを予約
		jumpBufferTimer_ = jumpBufferTime_;
	}
	else
	{
		jumpBufferTimer_ = std::max(0.0f, jumpBufferTimer_ - dt);
	}

	// Coyote
	if (IsGrounded()) coyoteTimer_ = coyoteTime_;
	else              coyoteTimer_ = std::max(0.0f, coyoteTimer_ - dt);

	// バッファが残っていて「いま跳べる」なら、このフレームの jumpPressed を強制発火
	if (jumpBufferTimer_ > 0.0f && (IsGrounded() || coyoteTimer_ > 0.0f))
	{
		in.jumpPressed = true;
		jumpBufferTimer_ = 0.0f;
	}
	else
	{
		// このフレームで本当に跳べないなら、押しっぱなし誤爆を避ける
		in.jumpPressed = false;
	}
}

void PlayerMotorComponent::Jump()
{
	if (IsGrounded() || coyoteTimer_ > 0.0f)
	{
		verticalVel_ = jumpSpeed_;

		// ジャンプ開始直後は非接地扱いにしておく
		grounded_ = false;

		coyoteTimer_ = 0.0f;
		jumpBufferTimer_ = 0.0f;
	}
}

void PlayerMotorComponent::SetLadderState(bool inLadderArea)
{
	// Trigger退出時だけ離脱ロックを解除し、範囲内Stayで即再捕捉されるのを防ぐ。
	if (!inLadderArea)
	{
		isInLadderArea_ = false;
		isClimbingLadder_ = false;
		ladderDetachLocked_ = false;
		return;
	}

	if (!ladderDetachLocked_)
	{
		isInLadderArea_ = true;
	}
}

void PlayerMotorComponent::StartBlink(float cameraYawRad, bool isAds)
{
	// 梯子Trigger内では昇降入力とBlinkが競合しないよう開始を拒否する。
	if (isInLadderArea_) return;

	// ADS中はダッシュしない
	if (isAds) return;

	// クールダウン中は開始しない
	if (blinkCooldownTimer_ > 0.0f) return;
	blinkCooldownTimer_ = blinkCooldown_;

	blinkTimer_ = blinkDuration_;

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

	blinkDirX_ = dx;
	blinkDirZ_ = dz;

	velX_ = blinkDirX_ * blinkSpeed_;
	velZ_ = blinkDirZ_ * blinkSpeed_;
}

void PlayerMotorComponent::Simulate(float dt, float cameraYawRad, LocoId locoId, bool isAds, bool isReloading)
{
	if (!tr_) return;
	stageOBBHitCount_ = 0;
	lastStageHitType_ = K4E::StageHitType::None;
	lastStageHitShape_ = K4E::StageHitShape::None;
	lastStageCorrectionAxis_ = K4E::StageCorrectionAxis::None;
	lastGroundedByStageTop_ = false;

	// デバッグ用
	const Vector3 oldPosition = tr_->translate_;

	// Resolve が返した接地情報を最後まで保持する
	bool resolvedGrounded = false;

	// ------------------------------------------------------------
	// 最初に地面情報更新
	// ------------------------------------------------------------
	UpdateGroundFromQuery();

	// ------------------------------------------------------------
	// yaw から forward / right を作る
	// ------------------------------------------------------------
	const float s = -std::sinf(cameraYawRad);
	const float c = std::cosf(cameraYawRad);

	const float fwdX = s;
	const float fwdZ = c;
	const float rightX = c;
	const float rightZ = -s;

	// ------------------------------------------------------------
	// 入力をワールド方向へ変換
	// ------------------------------------------------------------
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

	// ------------------------------------------------------------
	// ダッシュ継続判定
	// ここを locoId 依存ではなく blinkTimer_ 依存にするのが重要
	// → Jump/Fall 中でもダッシュジャンプの勢いを維持できる
	// ------------------------------------------------------------
	const bool blinkActive = (!isInLadderArea_ && !isAds && blinkTimer_ > 0.0f);

	// ------------------------------------------------------------
	// 水平速度
	// ------------------------------------------------------------
	if (isInLadderArea_)
	{
		// 梯子中は水平移動を止め、W/SまたはShift/Ctrlを純粋な上下入力として扱う。
		velX_ = 0.0f;
		velZ_ = 0.0f;
	}
	else if (blinkActive)
	{
		blinkTimer_ = std::max(0.0f, blinkTimer_ - dt);

		// ダッシュ中は固定速度
		velX_ = blinkDirX_ * blinkSpeed_;
		velZ_ = blinkDirZ_ * blinkSpeed_;
	}
	else
	{
		// ADS中にダッシュ残りがあってもここで切る
		if (isAds)
		{
			blinkTimer_ = 0.0f;
		}

		float speed = 0.0f;

		switch (locoId)
		{
		case LocoId::Walk:
			speed = walkSpeed_;
			break;

		case LocoId::Run:
			speed = runSpeed_;
			break;

		case LocoId::Land:
			speed = sprint_ ? runSpeed_ : walkSpeed_;
			break;

		case LocoId::Jump:
		case LocoId::Fall:
			// 空中でも sprint 状態を維持
			speed = sprint_ ? runSpeed_ : walkSpeed_;
			break;

		case LocoId::Idle:
		default:
			speed = 0.0f;
			break;
		}

		const bool groundedNow = IsGrounded();
		const bool justLeftGround = (wasGroundedLastFrame_ && !groundedNow);

		// 走行ジャンプした瞬間だけ少しブースト
		if (justLeftGround && sprint_ && locoId == LocoId::Jump)
		{
			speed *= sprintJumpBoostMul_;
		}

		// 地上は少しだけ減衰、空中は保持
		if (groundedNow)
		{
			speed *= groundMoveMul_;
		}
		else
		{
			speed *= airMoveMul_;
		}

		if (isAds)       speed *= adsMoveMul_;
		if (isReloading) speed *= reloadMoveMul_;

		// Minecraft寄り：入力がなければ即0
		velX_ = moveDirX * speed;
		velZ_ = moveDirZ * speed;
	}

	// ------------------------------------------------------------
	// 水平移動
	// ------------------------------------------------------------
	tr_->translate_.x += velX_ * dt;
	tr_->translate_.z += velZ_ * dt;

	UpdateGroundFromQuery();

	// ------------------------------------------------------------
	// 縦移動
	// ------------------------------------------------------------
	if (isInLadderArea_)
	{
		// 無入力時はY速度を0にしてその場へ留まり、入力時だけ調整可能速度で昇降する。
		float climbInput = moveZ_;
		if (ladderDescendHeld_)
		{
			climbInput = -1.0f;
		}
		verticalVel_ = std::clamp(climbInput, -1.0f, 1.0f) * ladderClimbSpeed_;
		isClimbingLadder_ = std::abs(climbInput) > 0.01f;
		grounded_ = false;
	}
	else
	{
		// 梯子外では既存の重力積分をそのまま使用する。
		verticalVel_ -= gravity_ * dt;
		isClimbingLadder_ = false;
	}
	tr_->translate_.y += verticalVel_ * dt;

	// ------------------------------------------------------------
	// ワールド衝突解決
	// ------------------------------------------------------------
	if (worldAABBs_ && !worldAABBs_->empty() && !IsOutsideStage())
	{
		// Stage外ではResolverの接地・位置補正を通さず、そのまま下方向へ落下させる。
		float vy = verticalVel_;

		const K4E::WorldCollisionResult r =
			K4E::WorldCollisionResolver::Resolve(
				*worldAABBs_,
				worldCollisionSettings_,
				oldPosition,
				tr_->translate_,
				true,
				&vy,
				wallObstacleAABBs_,
				wallObstacleOBBs_,
				wallObstacleWalkable_);

		// fixedCenter は物理中心なので描画座標へ戻す
		tr_->translate_ = r.fixedCenter + worldCollisionSettings_.centerOffset;
		verticalVel_ = vy;
		resolvedGrounded = r.grounded;
		// Debug表示へ渡すため、OBB NarrowPhaseで実際に押し戻した障害物数を保存する。
		stageOBBHitCount_ = r.obbHitCount;
		lastStageHitType_ = r.lastHitType;
		lastStageHitShape_ = r.lastHitShape;
		lastStageCorrectionAxis_ = r.lastCorrectionAxis;
		lastGroundedByStageTop_ = r.groundedByStageTop;
	}

	// ------------------------------------------------------------
	// 最後に地面情報を再取得
	// ------------------------------------------------------------
	UpdateGroundFromQuery();

	// ------------------------------------------------------------
	// grounded 判定
	// Resolve の grounded を優先的に活かす
	// ------------------------------------------------------------
	const bool insideStage = !IsOutsideStage();
	const bool queryGrounded = insideStage &&
		(tr_->translate_.y <= groundY_ + groundSnapEpsilon_) &&
		(verticalVel_ <= 0.0f);

	if (!isInLadderArea_ && insideStage && (resolvedGrounded || queryGrounded))
	{
		if (tr_->translate_.y < groundY_)
		{
			tr_->translate_.y = groundY_;
		}

		if (verticalVel_ < 0.0f)
		{
			verticalVel_ = 0.0f;
		}

		grounded_ = true;
	}
	else
	{
		grounded_ = false;
	}

	// ------------------------------------------------------------
	// デバッグ速度
	// ------------------------------------------------------------
	const Vector3 delta = tr_->translate_ - oldPosition;
	const float invDt = (dt > 1e-6f) ? (1.0f / dt) : 0.0f;

	dbgSpeedXZ_ = std::sqrt(delta.x * delta.x + delta.z * delta.z) * invDt;
	dbgSpeedY_ = delta.y * invDt;

	prevPos_ = tr_->translate_;
	prevPosValid_ = true;

	wasGroundedLastFrame_ = grounded_;
}
