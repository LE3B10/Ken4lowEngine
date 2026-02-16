#define NOMINMAX
#include "Player.h"
#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "Camera.h"             
#include "InputSnapshot.h"
#include "CollisionManager.h"
#include <PostEffectManager.h>
#include <VignetteEffect.h>
#include <RadialBlurEffect.h>

#include <cmath>                
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include "PlayerHurtbox.h"
#include "PlayerInputSnapshot.h"
#include "PlayerStateMachines.h"

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

	// WeaponMasterData をロードして現在武器を適用
	LoadWeaponMasterDataOnce();
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update(float deltaTime)
{
	if (isDebugCamera_)
	{
		BaseCharacter::Update(deltaTime); // デバッグカメラ中はベースのUpdateも呼ぶ（移動や回転を反映させるため）
		return; // デバッグカメラ中は以降の処理をスキップ
	}
	if (!input_) { BaseCharacter::Update(deltaTime); return; }

	// Snapshot生成
	inputSnap_ = BuildInputSnapshot(*input_);

	// ---- Fire mode toggle (V) ----
	if (weaponLoaded_ && inputSnap_.toggleFireModePressed)
	{
		weaponSys_.Weapon().ToggleFireMode();
	}

	// ---- Melee category: LMB attacks (disable gun fire) ----
	if (weaponCategory_ == EWeaponCategory::Melee)
	{
		// Fキー近接に加えて、左クリックでも近接できるようにする
		inputSnap_.meleePressed = inputSnap_.meleePressed || inputSnap_.firePressed;
		inputSnap_.fireHeld = false;
		inputSnap_.firePressed = false;
		inputSnap_.aimHeld = false;
		inputSnap_.aimPressed = false;
	}

	// Weapon（クールダウン/リロード/バースト/拡散）
	TickWeapon(deltaTime);


	// カテゴリ切替（数字キー1..6）
	if (inputSnap_.weaponSlotPressed != 0)
	{
		EWeaponCategory cat = weaponCategory_;
		switch (inputSnap_.weaponSlotPressed)
		{
		case 1: cat = EWeaponCategory::Primary; break;
		case 2: cat = EWeaponCategory::Backup; break;
		case 3: cat = EWeaponCategory::Melee; break;
		case 4: cat = EWeaponCategory::Special; break;
		case 5: cat = EWeaponCategory::Sniper; break;
		case 6: cat = EWeaponCategory::Heavy; break;
		default: break;
		}

		if (cat != weaponCategory_)
		{
			SwitchWeaponCategory(cat);
		}
	}

	// 武器切替（ホイール/DPAD想定）
	if (inputSnap_.weaponSwitch != 0)
	{
		SwitchWeaponByDelta(inputSnap_.weaponSwitch);
	}

	// カメラ角度を先に更新（＝移動方向の基準になる）
	fpsCamera_.SetDeltaTime(deltaTime);
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

	// 親子描画更新
	BaseCharacter::Update(deltaTime);

	SyncHurtboxes();
	UpdateDamagePostEffect(deltaTime);
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

	if (ImGui::CollapsingHeader("Weapon", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("MasterDir: %s", weaponMasterDir_.string().c_str());

		if (!weaponLoaded_)
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "WeaponMasterData: NOT LOADED");
			if (!weaponLoadError_.empty())
				ImGui::TextWrapped("%s", weaponLoadError_.c_str());

			if (ImGui::Button("Load WeaponMasterData"))
				LoadWeaponMasterDataOnce();
		}
		else
		{
			const auto& w = weaponSys_.Weapon();
			const auto& p = w.Params();
			const auto& s = w.State();

			ImGui::Text("WeaponID: %d", currentWeaponId_);
			ImGui::Text("DefaultAutomatic: %s", p.isAutomatic ? "true" : "false");
			ImGui::Text("CurrentFireMode: %s", w.IsAutomatic() ? "auto" : "semi");
			ImGui::Text("CanToggleMode: %s", p.canToggleFireMode ? "true" : "false");
			ImGui::Text("Damage: %.2f", p.damage);
			ImGui::Text("ProjectileSpeed: %.2f", p.projectileSpeed);
			ImGui::Text("SecPerShot: %.3fs", p.secPerShot);
			ImGui::Text("ReloadSec: %.2fs", p.reloadSec);

			ImGui::Separator();
			ImGui::Text("Ammo: %d / %d   (Reserve: %d)", s.magAmmo, p.magCapacity, s.reserveAmmo);
			ImGui::Text("Reloading: %s (%.2fs)", s.isReloading ? "true" : "false", s.reloadTimer);
			ImGui::Text("Cooldown: %.3fs", s.fireCooldown);
			ImGui::Text("Burst: rem=%d  timer=%.3fs", s.burstRemaining, s.burstTimer);
			ImGui::Text("Spread: %.3f", s.spread);

			static int sEquipID = 0;
			ImGui::InputInt("EquipID", &sEquipID);
			if (ImGui::Button("Equip"))
				EquipWeaponByID(sEquipID);

			ImGui::SameLine();
			if (ImGui::Button("Reload WeaponMasterData"))
			{
				weaponLoaded_ = false;
				weaponLoadError_.clear();
				LoadWeaponMasterDataOnce();
			}
		}
	}

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

	StartDamagePostEffect(dmg);

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

\
void Player::StartDamagePostEffect(float damage)
{
	// ダメージ量から 0..1 の強さへ（HP100基準で 5〜25ダメージくらいが気持ちいい）
	const float s = std::clamp((damage / std::max(1.0f, maxHp_)) * 4.0f, 0.15f, 1.0f);

	// 連続被弾は「上書き＋強さは最大」を採用
	damagePostTimer_ = damagePostDuration_;
	damagePostStrength_ = std::max(damagePostStrength_, s);
}

void Player::UpdateDamagePostEffect(float dt)
{
	auto* pem = K4E::PostEffectManager::GetInstance();
	if (!pem) return;

	// エフェクト取得（無ければ何もしない）
	auto* vig = dynamic_cast<K4E::VignetteEffect*>(pem->GetEffect("VignetteEffect"));
	auto* blur = dynamic_cast<K4E::RadialBlurEffect*>(pem->GetEffect("RadialBlurEffect"));
	if (!vig || !blur) return;

	// 初回だけ「元の値」を保存（終わったら戻すため）
	if (!damagePostCapturedBase_)
	{
		baseVignettePower_ = vig->GetPower();
		baseVignetteRange_ = vig->GetRange();
		baseRadialBlurStrength_ = blur->GetBlurStrength();
		baseRadialBlurSamples_ = blur->GetSampleCount();
		damagePostCapturedBase_ = true;
	}

	if (damagePostTimer_ > 0.0f)
	{
		damagePostTimer_ -= dt;
		if (damagePostTimer_ < 0.0f) damagePostTimer_ = 0.0f;

		// 1 -> 0 で減衰（開始が一番強い）
		const float t = (damagePostDuration_ > 0.0f) ? (damagePostTimer_ / damagePostDuration_) : 0.0f;
		const float ease = t * t; // smooth fade-out
		const float amp = ease * damagePostStrength_;

		// 被弾中だけ有効化（ImGuiで常時ONにしてる場合は effectEnabled_ が優先される）
		pem->EnableEffect("VignetteEffect");
		pem->EnableEffect("RadialBlurEffect");

		// ---- パラメータ（好みに合わせて調整OK） ----
		const float maxVignettePower = 2.0f;   // 0.8 -> 2.0 くらい
		const float maxVignetteRange = 0.70f;  // 0.5 -> 0.7 くらい
		const float maxBlurStrength = 0.65f;  // 0.3 -> 0.65 くらい
		const float blurSamples = 16.0f;

		vig->SetPower(baseVignettePower_ + (maxVignettePower - baseVignettePower_) * amp);
		vig->SetRange(baseVignetteRange_ + (maxVignetteRange - baseVignetteRange_) * amp);

		blur->SetCenter(K4E::Vector2(0.5f, 0.5f));
		blur->SetSampleCount(blurSamples);
		blur->SetBlurStrength(baseRadialBlurStrength_ + (maxBlurStrength - baseRadialBlurStrength_) * amp);
	}
	else
	{
		// 終了：元の値へ戻す
		vig->SetPower(baseVignettePower_);
		vig->SetRange(baseVignetteRange_);
		blur->SetSampleCount(baseRadialBlurSamples_);
		blur->SetBlurStrength(baseRadialBlurStrength_);

		// プログラム側フラグだけOFF（ImGuiでONならそのまま残る）
		pem->DisableEffect("VignetteEffect");
		pem->DisableEffect("RadialBlurEffect");

		damagePostStrength_ = 0.0f;
	}
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

// ---- combat（WeaponSystem/WeaponInstance に委譲）----
bool Player::FSM_CanFire() const
{
	if (!weaponLoaded_) return false;
	const auto& w = weaponSys_.Weapon();
	const auto& p = w.Params();
	const auto& s = w.State();

	if (s.isReloading) return false;
	if (s.fireCooldown > 0.0f) return false;
	if (s.magAmmo < p.ammoPerShot) return false;
	if (s.burstRemaining > 0 && s.burstTimer > 0.0f) return false;

	// 入力ゲート（発射モードに応じて判定）
	const bool autoMode = w.IsAutomatic();
	if (autoMode)
	{
		if (!inputSnap_.fireHeld) return false;
	}
	else
	{
		if (!inputSnap_.firePressed) return false;
	}

	return true;
}
void Player::FSM_FireOnce()
{
	// NOTE: CombatFSM から呼ばれる発射処理は「必ずここ」を通す
	if (!weaponLoaded_) return;
	if (!bulletManager_ || !shootCamera_) return;

	// TryFire は内部で (auto/semiauto)・クールダウン・残弾を処理してくれる
	const auto before = weaponSys_.Weapon().State();
	weaponSys_.Weapon().TryFire(inputSnap_.fireHeld, inputSnap_.firePressed, shootCamera_, bulletManager_, collisionManager_);
	const auto& after = weaponSys_.Weapon().State();

	const bool fired = (after.magAmmo != before.magAmmo) || (after.fireCooldown > before.fireCooldown);
	if (!fired) return;

	// ---- デバッグ用のショットレイ（命中判定とは別） ----
	if (auto* cam = fpsCamera_.GetCamera())
	{
		K4E::Segment seg{};
		seg.origin = cam->GetTranslate();
		seg.diff = cam->GetForward() * hitscanRange_;
		SetSegment(seg);
		shotDebugTimer_ = 0.05f;
	}
}

bool Player::FSM_IsReloadFinished() const
{
	if (!weaponLoaded_) return true;
	return !weaponSys_.Weapon().State().isReloading;
}

void Player::FSM_StartReload()
{
	if (!weaponLoaded_) return;
	weaponSys_.Weapon().StartReload();
}

bool Player::FSM_IsMeleeFinished() const { return true; } // TODO: 近接実装時に置換
void Player::FSM_StartMelee() {}                           // TODO: 近接実装時に置換

void Player::FSM_SetAiming(bool on)
{
	fpsCamera_.SetAiming(on);
}

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

bool Player::EquipWeaponByID(int32_t weaponID)
{
	if (!weaponLoaded_) LoadWeaponMasterDataOnce();
	if (!weaponLoaded_) return false;

	std::string err;
	if (!weaponSys_.EquipById(weaponID, &err))
	{
		weaponLoadError_ = err;
		return false;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
	return true;
}

bool Player::LoadWeaponMasterDataOnce()
{
	if (weaponLoaded_) return true;

	// ディレクトリが違う/作業ディレクトリが環境で変わることがあるので候補を順に試す
	std::vector<std::filesystem::path> candidates;
	candidates.push_back(weaponMasterDir_);
	auto addCandidate = [&](const char* p)
		{
			if (weaponMasterDir_ != p) candidates.push_back(p);
		};
	// 新しい配置（推奨）: Resources/JSON/weapons/[category]/*.json
	addCandidate("Resources/JSON/weapons");
	addCandidate("JSON/weapons");
	// 旧配置（互換）
	addCandidate("Resources/WeaponMasterData");
	addCandidate("WeaponMasterData");

	std::string err;
	bool loaded = false;
	for (const auto& dir : candidates)
	{
		if (dir.empty()) continue;
		if (!std::filesystem::exists(dir)) continue;
		if (weaponSys_.Load(dir, &err))
		{
			weaponMasterDir_ = dir;
			loaded = true;
			break;
		}
	}

	if (!loaded)
	{
		weaponLoaded_ = false;
		weaponLoadError_ = err.empty() ? "WeaponMasterData: 読み込みに失敗しました（ディレクトリを確認してください）" : err;
		currentWeaponId_ = 0;
		weaponIdList_.clear();
		return false;
	}

	weaponLoaded_ = true;

	// まずはカテゴリ（デフォルトPrimary）だけをプレイヤーの選択リストにする
	weaponIdList_ = weaponSys_.GetWeaponIdListSortedByCategory(weaponCategory_);
	if (weaponIdList_.empty())
	{
		// そのカテゴリが空なら、全カテゴリのIDリストにフォールバック（データがないと誤認しないため）
		weaponIdList_ = weaponSys_.GetWeaponIdListSorted();
	}
	if (weaponIdList_.empty())
	{
		weaponLoaded_ = false;
		weaponLoadError_ = "WeaponMasterData: データが0件でした。（Resources/JSON/weapons/primary などにjsonがあるか確認してください）";
		currentWeaponId_ = 0;
		return false;
	}

	// 既にIDがあるならそのIDを、なければ先頭を装備
	std::string equipErr;
	if (currentWeaponId_ > 0)
	{
		if (!weaponSys_.EquipById(currentWeaponId_, &equipErr))
			weaponSys_.EquipFirst(&equipErr);
	}
	else
	{
		weaponSys_.EquipFirst(&equipErr);
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	if (!equipErr.empty())
		weaponLoadError_ = equipErr;
	else
		weaponLoadError_.clear();

	return true;
}

void Player::SwitchWeaponByDelta(int delta)
{
	if (!weaponLoaded_) LoadWeaponMasterDataOnce();
	if (!weaponLoaded_) return;
	if (weaponIdList_.empty()) return;

	// 現在IDのindexを探す
	int idx = 0;
	for (int i = 0; i < static_cast<int>(weaponIdList_.size()); ++i)
	{
		if (weaponIdList_[i] == currentWeaponId_) { idx = i; break; }
	}

	idx += (delta > 0) ? 1 : -1;
	if (idx < 0) idx = static_cast<int>(weaponIdList_.size()) - 1;
	if (idx >= static_cast<int>(weaponIdList_.size())) idx = 0;

	EquipWeaponByID(weaponIdList_[idx]);
}

void Player::SwitchWeaponCategory(EWeaponCategory category)
{
	// まずロード（失敗したら何もしない）
	if (!LoadWeaponMasterDataOnce())
		return;

	// 現カテゴリの最後の武器IDを記録
	const int curIdx = static_cast<int>(weaponCategory_);
	if (curIdx >= 0 && curIdx < static_cast<int>(lastWeaponIdByCategory_.size()))
		lastWeaponIdByCategory_[curIdx] = currentWeaponId_;

	// 新カテゴリのID一覧を作る
	const auto ids = weaponSys_.GetWeaponIdListSortedByCategory(category);
	if (ids.empty())
	{
		weaponLoadError_ = "WeaponMasterData: このカテゴリに武器データがありません。";
		return;
	}

	weaponCategory_ = category;
	weaponIdList_ = ids;

	// 新カテゴリで前回使ってた武器があれば優先、なければ先頭
	int32_t targetId = weaponIdList_.front();
	const int newIdx = static_cast<int>(weaponCategory_);
	if (newIdx >= 0 && newIdx < static_cast<int>(lastWeaponIdByCategory_.size()))
	{
		const int32_t last = lastWeaponIdByCategory_[newIdx];
		if (last > 0 && std::find(weaponIdList_.begin(), weaponIdList_.end(), last) != weaponIdList_.end())
			targetId = last;
	}

	std::string err;
	if (!weaponSys_.EquipById(targetId, &err))
	{
		weaponLoadError_ = err.empty() ? "WeaponMasterData: Equipに失敗しました。" : err;
		return;
	}

	currentWeaponId_ = weaponSys_.GetEquippedWeaponId();
	weaponLoadError_.clear();
}


void Player::TickWeapon(float dt)
{
	if (!weaponLoaded_) return;
	weaponSys_.Tick(dt);
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
	apply(1, parts_[idx.head].transform.worldTranslate_ + K4E::Vector3(0.0f, 0.5f, 0.0f), parts_[idx.head].transform.worldRotate_);
	apply(2, parts_[idx.leftArm].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.leftArm].transform.worldRotate_);
	apply(3, parts_[idx.rightArm].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.rightArm].transform.worldRotate_);
	apply(4, parts_[idx.leftLeg].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.leftLeg].transform.worldRotate_);
	apply(5, parts_[idx.rightLeg].transform.worldTranslate_ + K4E::Vector3(0.0f, -0.75f, 0.0f), parts_[idx.rightLeg].transform.worldRotate_);
}
