#define NOMINMAX
#include "Player.h"
#include "Bullet.h"
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

static K4E::Vector3 RotateByEuler(const K4E::Vector3& v, const K4E::Vector3& rot)
{
	// エンジン側のMatrix4x4::MakeRotateMatrixが使える前提
	const auto m = K4E::Matrix4x4::MakeRotateMatrix(rot);

	// 行/列の定義が環境で違うことがあるので、もし方向が変なら下の式を入れ替えてOK
	return {
		v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
		v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
		v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2],
	};
}

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

	// hurtbox作成（部位ごと）
	const uint32_t hurtType = (uint32_t)CollisionTypeIdDef::kPlayer;
	// ↑まず最小はこれでOK（同じkPlayerバケットに入れて判定させる）
	// ちゃんと分けたいなら kPlayerHurtbox を enum に追加して typeId を変える

	auto make = [&](int idx, PlayerHitPart part, float mul, K4E::Vector3 half)
		{
			hurtboxes_[idx] = std::make_unique<PlayerHurtbox>();
			hurtboxes_[idx]->Initialize(this, part, mul, hurtType);
			hurtboxes_[idx]->SetOBBHalfSize(half);

			auto& t = hbTuning_[idx];
			t.halfSize = half;
			t.damageMul = mul;
			t.enabled = true;
		};

	make(0, PlayerHitPart::Body, 1.0f, { 0.5f, 0.75f, 0.25f });
	make(1, PlayerHitPart::Head, 2.0f, { 0.5f, 0.5f, 0.5f });
	make(2, PlayerHitPart::LeftArm, 1.0f, { 0.25, 0.75, 0.25 });
	make(3, PlayerHitPart::RightArm, 1.0f, { 0.25, 0.75, 0.25 });
	make(4, PlayerHitPart::LeftLeg, 1.0f, { 0.25, 0.75, 0.25 });
	make(5, PlayerHitPart::RightLeg, 1.0f, { 0.25, 0.75, 0.25 });

	// CollisionManagerに登録（Scene側でやってるなら不要）
	if (collisionManager_)
	{
		for (auto& hb : hurtboxes_) collisionManager_->AddCollider(hb.get());
	}

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
	if (isDebugCamera_)
	{
		BaseCharacter::Update(deltaTime); // デバッグカメラ中はベースのUpdateも呼ぶ（移動や回転を反映させるため）
		SyncHurtboxes(); // コライダー位置も合わせる
		return; // デバッグカメラ中は以降の処理をスキップ
	}

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

	// 衝突用コライダー同期（描画の親子付けとは別物なので毎フレーム合わせる）
	if (auto* tr = GetWorldTransform())
	{
		SetCenterPosition(tr->translate_);
		//SetOrientation(tr->rotate_);
	}

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

	SyncHurtboxes();
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

	static const char* kPartNames[] = { "Body","Head","LeftArm","RightArm","LeftLeg","RightLeg" };

	if (ImGui::CollapsingHeader("Player Hurtbox Tuning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("DebugDraw", &hbDebugDraw_);
		ImGui::Combo("Part", &hbSelected_, kPartNames, IM_ARRAYSIZE(kPartNames));

		auto& t = hbTuning_[hbSelected_];
		ImGui::Checkbox("Enabled", &t.enabled);
		ImGui::DragFloat3("LocalOffset", &t.localOffset.x, 0.01f);
		ImGui::DragFloat3("HalfSize", &t.halfSize.x, 0.01f, 0.01f, 10.0f);
		ImGui::DragFloat3("RotOffset", &t.rotOffset.x, 0.01f);
		ImGui::DragFloat("DamageMul", &t.damageMul, 0.01f, 0.1f, 10.0f);
	}

#endif // USE_IMGUI
}

/// -------------------------------------------------------------
///				　			衝突判定処理
/// -------------------------------------------------------------
void Player::OnCollision(K4E::Collider* other)
{
	if (!other) return;

	//const uint32_t otherType = other->GetTypeID();
	//const uint32_t kEnemyBullet = static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet);
	//const uint32_t kBossBullet = static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet);

	//if (otherType != kEnemyBullet && otherType != kBossBullet) return;

	// 多段ヒット防止（OnCollisionStay 対策）
	const uint32_t otherId = other->GetUniqueID();
	if (contactRecord_.Check(otherId)) return;
	contactRecord_.Add(otherId);

	//int dmg = 1;
	//if (auto* b = other->GetOwner<Bullet>())
	//{
	//	dmg = b->GetDamage();
	//}

	//hp_ -= static_cast<float>(dmg);
	//if (hp_ < 0.0f) hp_ = 0.0f;

	OnHitByEnemyBullet(other, PlayerHitPart::Body, 1.0f);
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

void Player::OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul)
{
	if (!bullet) return;

	const uint32_t otherType = bullet->GetTypeID();
	const uint32_t kEnemyBullet = static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet);
	const uint32_t kBossBullet = static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet);

	// 敵弾以外は無視
	if (otherType != kEnemyBullet && otherType != kBossBullet) return;

	// ---- 多段ヒット防止（重要）
	// 1発の弾が「頭」「腕」「胴」など複数Hurtboxに同フレームで当たっても1回だけにする
	const uint32_t bulletId = bullet->GetUniqueID();
	if (contactRecord_.Check(bulletId)) return;
	contactRecord_.Add(bulletId);

	// ---- ダメージ取得
	int baseDmg = 1;
	if (auto* b = bullet->GetOwner<Bullet>())
	{
		baseDmg = b->GetDamage();
	}

	// ---- 部位倍率（mulはHurtbox側で渡してる）
	float dmg = static_cast<float>(baseDmg) * mul;
	hp_ -= dmg;
	if (hp_ < 0.0f) hp_ = 0.0f;

	// ---- 任意：被弾スタン（入れたいなら）
	// PlayerBrain は status が Stunned だと loco/combat を止める設計なので相性が良い
	float stunSec = 0.08f; // 基本
	switch (part)
	{
	case PlayerHitPart::Head: stunSec = 0.15f; break; // ヘッドは少し長め
	default: break;
	}

	PlayerContext ctx{ api_, inputSnap_, 0.0f };
	brain_.status.RequestStun(ctx, stunSec);

	// スタン中に「前の移動状態」が残って動くのを防ぐ（おすすめ）
	moveX_ = 0.0f;
	moveZ_ = 0.0f;
	sprint_ = false;
	dashTimer_ = 0.0f;

	brain_.loco.Change(ctx, LocoId::Idle);
	brain_.combat.Change(ctx, CombatId::Hip);

	// TODO: hp_==0 のとき死亡処理を入れるならここ
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

void Player::SyncHurtboxes()
{
	if (!hbDebugDraw_) return;

	auto apply = [&](int hbIdx, const K4E::Vector3& worldPos, const K4E::Vector3& worldRot)
		{
			auto& t = hbTuning_[hbIdx];

			if (!t.enabled)
			{
				// 非表示（Collider::DrawはhalfSize≒0で描かない実装）
				hurtboxes_[hbIdx]->SetOBBHalfSize({ 0,0,0 });
				return;
			}

			K4E::Vector3 rot = worldRot;
			rot.y = -rot.y;

			hurtboxes_[hbIdx]->SetOBBHalfSize(t.halfSize);
			hurtboxes_[hbIdx]->SetOrientation(rot + t.rotOffset);

			const K4E::Vector3 offsetW = RotateByEuler(t.localOffset, rot);
			hurtboxes_[hbIdx]->SetCenterPosition(worldPos + offsetW);
		};

	// Body: body_.transform.translate_ / rotate_ でOK（親なし）
	apply(0, body_.transform.translate_, body_.transform.rotate_);

	// Parts: worldTranslate_ / worldRotate_ を必ず使う
	const auto idx = GetPartIndices();
	apply(1, parts_[idx.head].transform.worldTranslate_ + K4E::Vector3(0.0f,0.5f,0.0f), parts_[idx.head].transform.worldRotate_);
	apply(2, parts_[idx.leftArm].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.leftArm].transform.worldRotate_);
	apply(3, parts_[idx.rightArm].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.rightArm].transform.worldRotate_);
	apply(4, parts_[idx.leftLeg].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.leftLeg].transform.worldRotate_);
	apply(5, parts_[idx.rightLeg].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.rightLeg].transform.worldRotate_);
}
