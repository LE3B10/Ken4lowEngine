#define NOMINMAX
#include "Player.h"
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "BulletManager.h"      
#include "Camera.h"             
#include "InputSnapshot.h"
#include "CollisionManager.h"

#include <cmath>                

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// ベースキャラクター初期化
	BaseCharacter::Initialize();

	// 入力取得
	input_ = K4E::Input::GetInstance();

	// テクスチャの設定
	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	// ID登録
	K4E::Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	K4E::Collider::SetOwner<Player>(this);

	// 体力初期化
	hp_ = maxHp_;

	// 地面Y（今の立ち位置を地面扱い）
	groundY_ = body_.transform.translate_.y;

	// APIをPlayerに接続
	api_.player = this;

	// 最初のEnterを呼ぶ（ChangeでEnterが走る）
	InputSnapshot dummy{};
	PlayerContext ctx{ api_, dummy, 0.0f };
	brain_.status.Change(ctx, StatusId::Normal);
	brain_.loco.Change(ctx, LocoId::Idle);
	brain_.combat.Change(ctx, CombatId::Hip);

	// カメラ初期化
	fpsCamera_.Initialize(api_.player);

	// シュートカメラを設定
	shootCamera_ = fpsCamera_.GetCamera();

	// 一人称なら体を非表示に
	SetFirstPersonView(fpsCamera_.GetViewMode() == K4E::FpsCamera::ViewMode::FirstPerson);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update(float deltaTime)
{
	if (!input_) { BaseCharacter::Update(deltaTime); return; }

	// Snapshot生成
	inputSnap_ = BuildInputSnapshot(*input_);

	// カメラ角度を先に更新（＝移動方向の基準になる）
	fpsCamera_.SetDeltaTime(deltaTime);
	fpsCamera_.SetAiming(inputSnap_.aimHeld);   // いまは入力でOK。後でCombatFSMの状態で決めてもOK
	fpsCamera_.UpdateLook(inputSnap_);

	// 体の向きをカメラYawへ動悸
	body_.transform.rotate_.y = fpsCamera_.GetYaw();

	// FSM更新（SetMoveInputなどが呼ばれる）
	PlayerContext ctx{ api_, inputSnap_, deltaTime };
	brain_.Update(ctx);

	// 移動・重力などでプレイヤー位置更新
	SimulateLocomotion(deltaTime);

	// 最後にカメラをプレイヤーへ位置同期（行列更新もここで）
	fpsCamera_.SyncToPlayer();

	// 一人称なら体を非表示に
	SetFirstPersonView(fpsCamera_.GetViewMode() == K4E::FpsCamera::ViewMode::FirstPerson);

	if (input_ && input_->TriggerMouse(0))
	{
		FireOnce();
	}

	// 親子描画更新
	BaseCharacter::Update(deltaTime);
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// ベースキャラクター描画
	BaseCharacter::Draw();
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
#ifdef USE_IMGUI

	fpsCamera_.DrawImGui();

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　			衝突判定処理
/// -------------------------------------------------------------
void Player::OnCollision(K4E::Collider* other)
{
	(void)other;
}

bool PlayerAPI::IsGrounded() const { return player ? player->FSM_IsGrounded() : true; }
float PlayerAPI::VerticalVelocity() const { return player ? player->FSM_VerticalVelocity() : 0.0f; }

void PlayerAPI::SetMoveInput(float x, float z) { if (player) player->FSM_SetMoveInput(x, z); }
void PlayerAPI::SetSprint(bool on) { if (player) player->FSM_SetSprint(on); }
void PlayerAPI::Jump() { if (player) player->FSM_Jump(); }
void PlayerAPI::StartDash() { if (player) player->FSM_StartDash(); }
bool PlayerAPI::IsDashFinished() const { return player ? player->FSM_IsDashFinished() : true; }

bool PlayerAPI::CanFire() const { return player ? player->FSM_CanFire() : false; }
void PlayerAPI::FireOnce() { if (player) player->FSM_FireOnce(); }
bool PlayerAPI::IsReloadFinished() const { return player ? player->FSM_IsReloadFinished() : true; }
void PlayerAPI::StartReload() { if (player) player->FSM_StartReload(); }
bool PlayerAPI::IsMeleeFinished() const { return player ? player->FSM_IsMeleeFinished() : true; }
void PlayerAPI::StartMelee() { if (player) player->FSM_StartMelee(); }

void PlayerAPI::SetAiming(bool on) { if (player) player->FSM_SetAiming(on); }
void PlayerAPI::SetStunned(bool on) { if (player) player->FSM_SetStunned(on); }

void Player::SetFirstPersonView(bool enabled)
{
	if (isFirstPersonView_ == enabled) return;
	isFirstPersonView_ = enabled;
	ApplyFirstPersonRenderFlags();
}

bool Player::FSM_IsGrounded() const
{
	const auto* tr = BaseCharacter::GetWorldTransform();
	if (!tr) return true;
	return (tr->translate_.y <= groundY_ + 0.001f) && (verticalVel_ <= 0.001f);
}

float Player::FSM_VerticalVelocity() const { return verticalVel_; }

void Player::FSM_SetMoveInput(float x, float z) { moveX_ = x; moveZ_ = z; }
void Player::FSM_SetSprint(bool on) { sprint_ = on; }

void Player::FSM_Jump()
{
	if (FSM_IsGrounded())
	{
		verticalVel_ = jumpSpeed_;
	}
}

void Player::FSM_StartDash()
{
	dashTimer_ = dashDuration_;

	const float yaw = fpsCamera_.GetYaw();
	const float s = -std::sinf(yaw);
	const float c = std::cosf(yaw);

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
		// 入力ゼロなら “カメラ前” にダッシュ
		dx = fwdX; dz = fwdZ;
	}

	dashDirX_ = dx;
	dashDirZ_ = dz;
}

bool Player::FSM_IsDashFinished() const { return dashTimer_ <= 0.0f; }

// ---- combatは今はスタブ（後でWeapon実装したら置換）----
bool Player::FSM_CanFire() const { return true; }

void Player::FSM_FireOnce()
{
	auto* cam = fpsCamera_.GetCamera();
	if (!cam) return;

	K4E::Segment seg{};
	seg.origin = cam->GetTranslate();
	seg.diff = cam->GetForward() * hitscanRange_;

	SetSegment(seg);
	shotDebugTimer_ = 0.05f;

	K4E::Collider* hit = nullptr;

	const bool hitEnemy = collisionManager_ &&
		collisionManager_->SegmentCast((uint32_t)CollisionTypeIdDef::kEnemy, seg, &hit);

	const bool hitBoss = collisionManager_ &&
		collisionManager_->SegmentCast((uint32_t)CollisionTypeIdDef::kBoss, seg, &hit);

	if (hitEnemy || hitBoss)
	{
		// ダメージやヒット演出
	}
}

bool Player::FSM_IsReloadFinished() const { return true; }
void Player::FSM_StartReload() {}
bool Player::FSM_IsMeleeFinished() const { return true; }
void Player::FSM_StartMelee() {}
void Player::FSM_SetAiming(bool) {}
void Player::FSM_SetStunned(bool) {}

void Player::SimulateLocomotion(float dt)
{
	auto* tr = GetWorldTransform();
	if (!tr) return;

	// カメラYawから “平面 forward/right” を作る（+Z前, +X右の想定）
	const float yaw = fpsCamera_.GetYaw();
	const float s = -std::sinf(yaw);
	const float c = std::cosf(yaw);

	// forward=(sin,0,cos), right=(cos,0,-sin)
	const float fwdX = s, fwdZ = c;
	const float rightX = c, rightZ = -s;

	// 入力(moveX_/moveZ_) をワールド方向へ変換
	float moveDirX = rightX * moveX_ + fwdX * moveZ_;
	float moveDirZ = rightZ * moveX_ + fwdZ * moveZ_;

	// 正規化（入力が0なら0）
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

	// 横移動
	if (brain_.loco.id == LocoId::Dash)
	{
		tr->translate_.x += dashDirX_ * dashSpeed_ * dt;
		tr->translate_.z += dashDirZ_ * dashSpeed_ * dt;
		dashTimer_ = (dashTimer_ > dt) ? (dashTimer_ - dt) : 0.0f;
	}
	else
	{
		float speed = 0.0f;
		switch (brain_.loco.id)
		{
		case LocoId::Walk: speed = walkSpeed_; break;
		case LocoId::Run:  speed = runSpeed_;  break;
		case LocoId::Jump:
		case LocoId::Fall: speed = runSpeed_ * airControl_; break;
		default: break;
		}

		tr->translate_.x += moveDirX * speed * dt;
		tr->translate_.z += moveDirZ * speed * dt;
	}

	// 縦（重力）
	verticalVel_ -= gravity_ * dt;
	tr->translate_.y += verticalVel_ * dt;

	if (tr->translate_.y <= groundY_)
	{
		tr->translate_.y = groundY_;
		verticalVel_ = 0.0f;
	}
}

void Player::ApplyFirstPersonRenderFlags()
{
	if (isFirstPersonView_)
	{
		SetBodyActive(false);
		SetAllPartsActive(false);

		const auto idx = GetPartIndices().rightArm;
		SetPartActive(idx, true);
	}
	else
	{
		SetBodyActive(true);
		SetAllPartsActive(true);
	}
}

static K4E::Vector3 NormalizeSafe(const K4E::Vector3& v)
{
	const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
	if (lenSq <= 1e-6f) return { 0,0,1 };
	const float inv = 1.0f / std::sqrt(lenSq);
	return { v.x * inv, v.y * inv, v.z * inv };
}

void Player::FireOnce()
{
	if (!bulletManager_) return;
	if (!shootCamera_) return;

	K4E::Vector3 origin = shootCamera_->GetTranslate();
	K4E::Vector3 dir = NormalizeSafe(shootCamera_->GetForward());

	// 自分の頭/壁へのめり込み回避で少し前に出す
	origin = origin + dir * muzzleForwardOffset_;

	bulletManager_->Spawn(origin, dir, bulletSpeed_, bulletDamage_);
}
