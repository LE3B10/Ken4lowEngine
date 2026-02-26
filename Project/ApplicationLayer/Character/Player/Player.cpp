#define NOMINMAX
#include "Player.h"
#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "Camera.h"             
#include "InputSnapshot.h"
#include "CollisionManager.h"
#include "HUDManager.h"

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
	   m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
	   m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
	   m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z,
	};
}

static void ExtractAxes_Row(const K4E::Matrix4x4& R, K4E::Vector3& ax, K4E::Vector3& ay, K4E::Vector3& az)
{
	ax = { R.m[0][0], R.m[0][1], R.m[0][2] };
	ay = { R.m[1][0], R.m[1][1], R.m[1][2] };
	az = { R.m[2][0], R.m[2][1], R.m[2][2] };
	ax = K4E::Vector3::Normalize(ax);
	ay = K4E::Vector3::Normalize(ay);
	az = K4E::Vector3::Normalize(az);
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

			// 既定の中心オフセット（部位ローカル）
			// もし将来ロード等で値が入っている場合は上書きしない
			if (t.localOffset.x == 0.0f && t.localOffset.y == 0.0f && t.localOffset.z == 0.0f)
			{
				switch (part)
				{
				case PlayerHitPart::Body:
					t.localOffset = { 0.0f,0.0f, 0.0f };
					break;
				case PlayerHitPart::Head:
					t.localOffset = { 0.0f, t.halfSize.y, 0.0f };
					break;
				case PlayerHitPart::LeftArm:
				case PlayerHitPart::RightArm:
				case PlayerHitPart::LeftLeg:
				case PlayerHitPart::RightLeg:
					t.localOffset = { 0.0f, -t.halfSize.y, 0.0f };
					break;
				default:
					t.localOffset = { 0.0f, 0.0f, 0.0f };
					break;
				}
			}

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
	if (hudManager_)
	{
		hudManager_->SetHP(hp_, maxHp_);
	}

	// 演出（VFX）の初期化
	vfx_.Reset();

	// APIをPlayerに接続
	api_.player = this;

	// 最初のEnterを呼ぶ（ChangeでEnterが走る）
	InputSnapshot dummy{};
	PlayerContext ctx{ api_, dummy, 0.0f };
	brain_.status.Change(ctx, StatusId::Normal);
	brain_.loco.Change(ctx, LocoId::Idle);
	brain_.combat.Change(ctx, CombatId::Hip);

	// View（カメラ/表示）初期化
	view_.BindBodyTransform(GetWorldTransform());
	PlayerViewComponent::FirstPersonRenderHooks hooks{};
	hooks.SetBodyActive = [this](bool on) { this->SetBodyActive(on); };
	hooks.SetAllPartsActive = [this](bool on) { this->SetAllPartsActive(on); };
	hooks.SetPartActive = [this](int idx, bool on) { this->SetPartActive(idx, on); };
	hooks.GetLeftArmIndex = [&]() { return (int)GetPartIndices().leftArm; };
	hooks.GetRightArmIndex = [&]() { return (int)GetPartIndices().rightArm; };

	view_.BindFirstPersonRenderHooks(std::move(hooks));
	auto& parts = GetBodyParts();
	view_.BindArmTransforms(&parts[GetPartIndices().leftArm].transform, &parts[GetPartIndices().rightArm].transform);

	view_.Initialize(this);

	{
		auto* cam = view_.GetCamera();
		PlayerViewComponent::CameraFovHooks fovHooks{};

		fovHooks.GetFov = [cam]() { return cam->GetFovY(); };
		fovHooks.SetFov = [cam](float fov) { cam->SetFovY(fov); };

		view_.BindCameraFovHooks(std::move(fovHooks));
	}

	// WeaponMasterData をロードして現在武器を適用
	weapon_.LoadWeaponMasterDataOnce();

	prevLocoId_ = brain_.loco.id;
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

	// ---- 最近当たった弾IDの掃除（多段ヒット防止のTTL） ----
	TickRecentBulletHits(deltaTime);

	// Snapshot生成（raw）
	InputSnapshot rawSnap = BuildInputSnapshot(*input_);

	// ---- Weapon（トグル/近接リマップ/切替/内部Tick）をコンポーネントへ委譲 ----
	// ※武器側が「移動/ジャンプでリロードキャンセル」等の入力を見る可能性があるため、
	//   まず raw を渡して武器状態を更新する。
	weapon_.UpdateAndHandleInput(deltaTime, rawSnap);

	// ---- リロード中の移動制限/キャンセル ----
	// 仕様:
	//  - リロード中は走らない（= sprint/dash 無効）
	//  - リロード中はジャンプしない
	//  - ただし「走る/ジャンプ/ダッシュ」を入力したらリロードをキャンセルする
	bool isReloading = false;
	float reloadTimer = 0.0f;
	float reloadSec = 0.0f;
	weapon_.GetReloadUI(isReloading, reloadTimer, reloadSec);

	const bool wantsCancelReload = isReloading && (rawSnap.sprintHeld || rawSnap.jumpPressed || rawSnap.dashPressed);
	if (wantsCancelReload)
	{
		TryCancelReloadInternal();

		// FSM側もリロード状態から即復帰（体感の遅延をなくす）
		// ※武器側のキャンセル実装が無い場合でも、プレイヤー操作はすぐ戻せる。
		if (brain_.combat.id == CombatId::Reload)
		{
			PlayerContext cancelCtx{ api_, rawSnap, deltaTime };
			brain_.combat.Change(cancelCtx, rawSnap.aimHeld ? CombatId::Aim : CombatId::Hip);
		}

		// このフレームは「キャンセルした扱い」で制限をかけない
		isReloading = false;
	}

	// Snapshot（FSM / Motor / View に流す用）
	inputSnap_ = rawSnap;
	if (isReloading)
	{
		// 走る/ジャンプ/ダッシュを抑止（歩きは許可）
		inputSnap_.sprintHeld = false;
		inputSnap_.jumpPressed = false;
		inputSnap_.dashPressed = false;
	}

	auto* tr = GetWorldTransform();
	motor_.BindTransform(tr);
	motor_.PreprocessInput(inputSnap_, deltaTime);
	SetCenterPosition(tr->translate_);

	// View：カメラ角度を先に更新（＝移動方向の基準になる）
	view_.BindBodyTransform(tr);
	view_.SetAiming(inputSnap_.aimHeld);

	float adsFovDeg = 60.0f; // 仮のADS時FOV。将来武器ごとに変えたい場合は WeaponData に入れて weapon_ から取る。
	float adsSpeed = 10.0f; // 仮のADS時FOV変化速度。将来武器ごとに変えたい場合は WeaponData に入れて weapon_ から取る。
	if (weapon_.GetCurrentAdsViewTuning(adsFovDeg, adsSpeed))
	{
		view_.SetWeaponAdsTuning(adsFovDeg, adsSpeed);
	}

	float adsMoveMul = 0.85f; // fallback（WeaponMasterDataのデフォルトに寄せる）
	if (weapon_.GetCurrentAdsMoveMultiplier(adsMoveMul))
	{
		motor_.SetAdsMoveMultiplier(adsMoveMul);
	}
	else
	{
		motor_.SetAdsMoveMultiplier(adsMoveMul);
	}

	view_.UpdateLook(deltaTime, inputSnap_);

	// FSM更新（SetMoveInputなどが呼ばれる）
	PlayerContext ctx{ api_, inputSnap_, deltaTime };
	brain_.Update(ctx);

	const LocoId prev = prevLocoId_;
	const bool isAds = inputSnap_.aimHeld;
	const bool dashJustStarted = (brain_.loco.id == LocoId::Dash && prev != LocoId::Dash);
	const bool moving = (inputSnap_.moveX * inputSnap_.moveX + inputSnap_.moveZ * inputSnap_.moveZ) > 0.01f;

	const LocoId cur = brain_.loco.id;
	const bool isAirLike = (cur == LocoId::Jump || cur == LocoId::Fall || cur == LocoId::Land);

	if (cur == LocoId::Run) runCarry_ = true;
	if (prev == LocoId::Run && isAirLike) runCarry_ = false; // ジャンプした瞬間は走りの勢いを持ち越すが、空中で一度でも走り以外の状態になったら解除

	// ダッシュは別扱い
	if (cur == LocoId::Dash) runCarry_ = false;

	if (isAirLike)
	{
		if (!inputSnap_.sprintHeld || isAds || !moving) runCarry_ = false;
	}
	else
	{
		// 地上状態では Run 以外ならキャリー不要
		if (cur != LocoId::Run) runCarry_ = false;
	}

	const bool isRunningForFov = (cur == LocoId::Run) || (isAirLike && runCarry_);
	const bool isDashing = (cur == LocoId::Dash);

	// ---- HUD連携（クロスヘア移動状態 / 着地 / HP） ----
	if (hudManager_)
	{
		const bool crosshairMoving = moving || isDashing;
		const bool crosshairSprinting = (cur == LocoId::Run) || (cur == LocoId::Dash);
		const bool crosshairAirborne = (cur == LocoId::Jump || cur == LocoId::Fall);
		hudManager_->SetCrosshairMovementState(crosshairMoving, crosshairSprinting, crosshairAirborne);
		if (cur == LocoId::Land && prev != LocoId::Land)
		{
			hudManager_->NotifyCrosshairLanded();
		}
		hudManager_->SetHP(hp_, maxHp_);
	}

	motor_.Simulate(deltaTime, view_.GetYaw(), brain_.loco.id, isAds, isReloading);

	// 最後にカメラをプレイヤーへ位置同期（行列更新もここで）
	view_.UpdateMovementFov(deltaTime, isRunningForFov, isDashing, dashJustStarted);
	view_.SyncToPlayer();
	view_.SyncViewModeToFirstPersonFlag();

	prevLocoId_ = brain_.loco.id;

	// 親子描画更新
	BaseCharacter::Update(deltaTime);

	SyncHurtboxes();
	vfx_.Update(deltaTime);
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

	view_.DrawImGui();

	weapon_.DrawImGui();

	if (ImGui::CollapsingHeader("Recoil Tuning", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Pre-shot camera kick (deg)");
		ImGui::DragFloat("Hip Pitch", &recoilPitchDegHip_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Hip Yaw", &recoilYawDegHip_, 0.1f, 0.0f, 20.0f);
		ImGui::Separator();
		ImGui::DragFloat("ADS Pitch", &recoilPitchDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
		ImGui::DragFloat("ADS Yaw", &recoilYawDegAds_, 0.01f, 0.0f, 10.0f, "%.2f");
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

	// ここでは「弾っぽいもの」を一律で委譲。
	OnHitByEnemyBullet(other, PlayerHitPart::Body, 1.0f);
}

void Player::TryCancelReloadInternal()
{
	// WeaponComponent 側の実装差に耐えるため、存在する関数だけを呼ぶ。
	// （存在しない関数は requires によりコンパイル対象にならない）
	if constexpr (requires(decltype(weapon_) & w) { w.CancelReload(); })
	{
		weapon_.CancelReload();
	}
	else if constexpr (requires(decltype(weapon_) & w) { w.AbortReload(); })
	{
		weapon_.AbortReload();
	}
	else if constexpr (requires(decltype(weapon_) & w) { w.StopReload(); })
	{
		weapon_.StopReload();
	}
	else
	{
		// 何もできない場合はFSMだけ解除する（操作感は戻るが、武器側はリロード完了まで撃てない可能性がある）
	}
}

bool PlayerAPI::IsGrounded() const { return player ? player->FSM_IsGrounded() : true; }
float PlayerAPI::VerticalVelocity() const { return player ? player->FSM_VerticalVelocity() : 0.0f; }

void PlayerAPI::SetMoveInput(float x, float z) { if (player) player->FSM_SetMoveInput(x, z); }
void PlayerAPI::SetSprint(bool on) { if (player) player->FSM_SetSprint(on); }
void PlayerAPI::Jump() { if (player) player->FSM_Jump(); }
void PlayerAPI::StartDash() { if (player) player->FSM_StartDash(); }
bool PlayerAPI::CanStartDash() const { return player ? player->FSM_CanStartDash() : false; }
bool PlayerAPI::IsDashFinished() const { return player ? player->FSM_IsDashFinished() : true; }

bool PlayerAPI::CanFire() const { return player ? player->FSM_CanFire() : false; }
void PlayerAPI::FireOnce() { if (player) player->FSM_FireOnce(); }
bool PlayerAPI::IsReloadFinished() const { return player ? player->FSM_IsReloadFinished() : true; }
void PlayerAPI::StartReload() { if (player) player->FSM_StartReload(); }
bool PlayerAPI::IsMeleeFinished() const { return player ? player->FSM_IsMeleeFinished() : true; }
void PlayerAPI::StartMelee() { if (player) player->FSM_StartMelee(); }

void PlayerAPI::SetAiming(bool on) { if (player) player->FSM_SetAiming(on); }
void PlayerAPI::SetStunned(bool on) { if (player) player->FSM_SetStunned(on); }

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
	if (IsRecentBulletHit(bulletId)) return;
	MarkRecentBulletHit(bulletId);

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

	vfx_.OnDamaged(dmg, maxHp_);
	if (hudManager_)
	{
		float strength01 = (maxHp_ > 0.0f) ? (dmg / maxHp_) : 1.0f;
		if (strength01 < 0.10f) strength01 = 0.10f;
		if (strength01 > 1.00f) strength01 = 1.00f;
		hudManager_->SetHP(hp_, maxHp_);
		hudManager_->NotifyPlayerHit(strength01);
	}

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

	brain_.loco.Change(ctx, LocoId::Idle);
	brain_.combat.Change(ctx, CombatId::Hip);

	// TODO: hp_==0 のとき死亡処理を入れるならここ
}

void Player::NotifyEnemyHitUI(bool isHeadshot)
{
	if (hudManager_) hudManager_->NotifyEnemyHit(isHeadshot);
}

void Player::NotifyEnemyKillUI(bool isHeadshot)
{
	if (hudManager_) hudManager_->NotifyEnemyKill(isHeadshot);
}

bool Player::GetWeaponSlotHUD(WeaponSlot::HudSnapshot& out) const
{
	out = {};

	out.selectedIndex = weapon_.GetSelectedHot_barIndex();

	for (int i = 0; i < WeaponSlot::kSlotCount; ++i)
	{
		const auto view = weapon_.GetAmmoViewByHot_barIndex(i);

		out.slotStates[i].useAmmo = view.usesAmmo;
		out.slotStates[i].ammoInfo.currentAmmo = view.mag;
		out.slotStates[i].ammoInfo.reserveAmmo = view.reserve;
	}

	return true;
}

void Player::FSM_FireOnce()
{
	// NOTE: CombatFSM から呼ばれる発射処理は「必ずここ」を通す
	auto* shootCam = view_.GetShootCamera();
	if (!bulletManager_ || !shootCam) return;

	const bool fired = weapon_.TryFire(inputSnap_, shootCam, bulletManager_, collisionManager_);
	if (!fired) return;

	/// ---------- カメラリコイル処理 ---------- ///
	const bool ads = inputSnap_.aimHeld;
	const float vDeg = ads ? recoilPitchDegAds_ : recoilPitchDegHip_;
	const float hDeg = ads ? recoilYawDegAds_ : recoilYawDegHip_;
	view_.AddRecoil(vDeg, hDeg);

	// ---- デバッグ用のショットレイ（命中判定とは別） ----
	if (auto* cam = view_.GetCamera())
	{
		K4E::Segment seg{};
		seg.origin = cam->GetTranslate();
		seg.diff = cam->GetForward() * hitscanRange_;
		SetSegment(seg);
		shotDebugTimer_ = 0.05f;
	}
}

bool Player::FSM_IsMeleeFinished() const { return true; } // TODO: 近接実装時に置換
void Player::FSM_StartMelee() {}                           // TODO: 近接実装時に置換

void Player::FSM_SetStunned(bool) {}

void Player::TickRecentBulletHits(float dt)
{
	for (auto it = recentBulletHits_.begin(); it != recentBulletHits_.end(); )
	{
		it->second -= dt;
		if (it->second <= 0.0f) it = recentBulletHits_.erase(it);
		else ++it;
	}
}

void Player::MarkRecentBulletHit(uint32_t id)
{
	// 念のため上限。極端にIDが増える状況でも肥大化しないようにする。
	if (recentBulletHits_.size() > 256) recentBulletHits_.clear();
	recentBulletHits_[id] = recentBulletHitTTL_;
}

void Player::SyncHurtboxes()
{
	auto apply = [&](int hbIdx, const K4E::Vector3& pivotWorld, const K4E::Vector3& worldRotEuler)
		{
			auto& t = hbTuning_[hbIdx];

			if (!t.enabled)
			{
				hurtboxes_[hbIdx]->SetOBBHalfSize({ 0,0,0 });
				hurtboxes_[hbIdx]->ClearOBBBasis();
				return;
			}

			hurtboxes_[hbIdx]->SetOBBHalfSize(t.halfSize);

			const K4E::Vector3 rotOBB = worldRotEuler + t.rotOffset;
			const K4E::Matrix4x4 R = K4E::Matrix4x4::MakeRotateMatrix(rotOBB);

			K4E::Vector3 ax, ay, az;
			ExtractAxes_Row(R, ax, ay, az);

			const K4E::Vector3 center =
				pivotWorld +
				ax * t.localOffset.x +
				ay * t.localOffset.y +
				az * t.localOffset.z;

			hurtboxes_[hbIdx]->SetCenterPosition(center);

			// ここが「親子計算をOBBに適用」する決め手：軸を直接渡す
			hurtboxes_[hbIdx]->SetOBBBasis(ax, ay, az);

			// 念のため（他コードがGetOrientation参照しても破綻しない用）
			hurtboxes_[hbIdx]->SetOrientation(rotOBB);
		};

	// Body（pivotは体幹の位置）
	apply(0, body_.transform.translate_, body_.transform.rotate_);

	// Parts（pivotは各部位のワールド位置）
	const auto idx = GetPartIndices();
	apply(1, parts_[idx.head].transform.worldTranslate_, parts_[idx.head].transform.worldRotate_);
	apply(2, parts_[idx.leftArm].transform.worldTranslate_, parts_[idx.leftArm].transform.worldRotate_);
	apply(3, parts_[idx.rightArm].transform.worldTranslate_, parts_[idx.rightArm].transform.worldRotate_);
	apply(4, parts_[idx.leftLeg].transform.worldTranslate_, parts_[idx.leftLeg].transform.worldRotate_);
	apply(5, parts_[idx.rightLeg].transform.worldTranslate_, parts_[idx.rightLeg].transform.worldRotate_);
}