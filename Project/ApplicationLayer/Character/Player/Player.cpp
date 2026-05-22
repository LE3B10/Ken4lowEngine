#define NOMINMAX
#include "Player.h"
#include "Bullet.h"
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include "Camera.h"
#include "InputSnapshot.h"
#include "CollisionManager.h"
#include "HUDManager.h"
#include "GpuParticleManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include "PlayerHurtbox.h"
#include "PlayerInputSnapshot.h"
#include "PlayerStateMachines.h"
#include "PlayerBrainComponent.h"
#include "PlayerDeathComponent.h"
#include "PlayerHurtboxComponent.h"
#include "PlayerWeaponController.h"
#include "PlayerCombatComponent.h"

namespace
{
	static bool IsMovingInput(const InputSnapshot& in)
	{
		return (in.moveX * in.moveX + in.moveZ * in.moveZ) > 0.01f;
	}

	static bool IsAirLikeLoco(LocoId loco)
	{
		return (loco == LocoId::Jump || loco == LocoId::Fall || loco == LocoId::Land);
	}

	static void SuppressActionInputDuringReload(InputSnapshot& in)
	{
		// リロード中はリロード演出を優先する。
		// ADS・ジャンプ・ダッシュ・ブリンク・射撃・近接・武器切替を受け付けると、
		// 腕と武器のポーズが混ざって崩れやすいため、入力だけ無効化する。
		in.sprintHeld = false;
		in.jumpHeld = false;
		in.jumpPressed = false;
		in.blinkPressed = false;

		in.aimHeld = false;
		in.aimPressed = false;

		in.fireHeld = false;
		in.firePressed = false;
		in.reloadPressed = false;
		in.meleePressed = false;

		in.weaponSwitch = 0;
		in.weaponSlotPressed = 0;
		in.toggleFireModePressed = false;
	}
}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	BaseCharacter::Initialize();

	{
		auto* tr = GetWorldTransform();
		if (tr)
		{
			const bool hasOffset =
				(spawnOffset_.x != 0.0f) || (spawnOffset_.y != 0.0f) || (spawnOffset_.z != 0.0f);

			if (hasSpawnPos_ || hasOffset)
			{
				if (hasSpawnPos_) tr->translate_ = spawnPos_ + spawnOffset_;
				else              tr->translate_ = tr->translate_ + spawnOffset_;

				SetCenterPosition(tr->translate_);
			}
		}
	}

	if (!refs_.input)
	{
		refs_.input = K4E::Input::GetInstance();
	}

	BaseCharacter::ApplySkinToAllParts(skinTexturePath_);

	K4E::Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	K4E::Collider::SetOwner<Player>(this);

	hurtbox_.Initialize(*this, refs_.collisionManager);

	damage_.Initialize(10000.0f);
	if (refs_.hudManager)
	{
		refs_.hudManager->SetHP(damage_.GetHP(), damage_.GetMaxHP());
	}

	vfx_.Reset();

	api_.player = this;
	brainComponent_.Initialize(api_);

	view_.BindBodyTransform(GetWorldTransform());

	PlayerViewComponent::FirstPersonRenderHooks hooks{};
	hooks.SetBodyActive = [this](bool on) { this->SetBodyActive(on); };
	hooks.SetAllPartsActive = [this](bool on) { this->SetAllPartsActive(on); };
	hooks.SetPartActive = [this](int idx, bool on) { this->SetPartActive(idx, on); };
	hooks.GetLeftArmIndex = [&]() { return (int)GetPartIndices().leftArm; };
	hooks.GetRightArmIndex = [&]() { return (int)GetPartIndices().rightArm; };
	view_.BindFirstPersonRenderHooks(std::move(hooks));

	auto& parts = GetBodyParts();
	view_.BindArmTransforms(
		&parts[GetPartIndices().leftArm].transform,
		&parts[GetPartIndices().rightArm].transform);

	view_.Initialize(this);

	{
		auto* cam = view_.GetCamera();
		if (cam)
		{
			PlayerViewComponent::CameraFovHooks fovHooks{};
			fovHooks.GetFov = [cam]() { return cam->GetFovY(); };
			fovHooks.SetFov = [cam](float fov) { cam->SetFovY(fov); };
			view_.BindCameraFovHooks(std::move(fovHooks));
		}
	}

	weapon_.LoadWeaponMasterDataOnce();

	brainComponent_.SetPrevLocoId(brainComponent_.GetCurrentLocoId());

	weaponVisual_.Initialize();
	weaponVisual_.BindWeaponLogic(&weapon_);
	weaponVisual_.BindRightHandTransform(&parts[GetPartIndices().rightArm].transform);

	weaponController_.Initialize(&weapon_, &weaponVisual_, &brainComponent_, &api_);
	combat_.BindDependencies(&weapon_, &view_);
	combat_.BindWeaponVisual(&weaponVisual_);
	combat_.SetAudioCallbacks(&audio_.onFire, &audio_.onReload);

	// 近接
	melee_.BindDependencies(&view_, refs_.collisionManager);

	// 近接のヒットコールバックをセット
	melee_.SetOnHitCallback([this]() { this->NotifyEnemyHitUI(false); });
}

void Player::BindDependencies(const PlayerDependencies& deps)
{
	refs_ = deps.refs;
	audio_ = deps.audio;

	if (!refs_.input)
	{
		refs_.input = K4E::Input::GetInstance();
	}

	if (deps.shootCamera)
	{
		view_.SetShootCamera(deps.shootCamera);
	}

	melee_.BindDependencies(&view_, refs_.collisionManager);
}

/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update(float deltaTime)
{
	if (runtime_.isDebugCamera)
	{
		BaseCharacter::Update(deltaTime);

		// デバッグカメラ中はゲーム入力・移動更新を止めるが、
		// 武器ビジュアルだけは右腕の最新ワールド行列へ追従させる。
		// ここを更新しないと、BaseCharacter 側で右腕が更新された後も
		// 武器だけ前フレームの行列に残り、右腕から離れて見える。
		weaponVisual_.Update(deltaTime, inputSnap_.aimHeld);
		SyncHurtboxes();

		return;
	}

	if (!refs_.input)
	{
		BaseCharacter::Update(deltaTime);
		return;
	}

	if (death_.IsActive())
	{
		UpdateDeath(deltaTime);
		return;
	}

	damage_.Tick(deltaTime);
	combat_.Tick(deltaTime);
	melee_.Tick(deltaTime, GetWorldTransform()->translate_);

	UpdateInputAndWeapon(deltaTime);
	UpdateBrain(deltaTime);
	UpdateMovementAndView(deltaTime);
	UpdatePresentation(deltaTime);
	ApplyFallDamage(deltaTime);
}

void Player::UpdateInputAndWeapon(float deltaTime)
{
	InputFrameContext ctx = BuildInputFrameContext(deltaTime);
	UpdateWeaponBeforeMotor(deltaTime, ctx);
	FinalizeInputSnapshotForGameplay(deltaTime, ctx);
	ApplyWeaponCameraAndMovementTuning();
}

Player::InputFrameContext Player::BuildInputFrameContext(float /*deltaTime*/)
{
	InputFrameContext ctx{};
	ctx.rawSnap = BuildInputSnapshot(*refs_.input);
	return ctx;
}

void Player::UpdateWeaponBeforeMotor(float deltaTime, InputFrameContext& ctx)
{
	bool wasReloading = false;
	float previousReloadTimer = 0.0f;
	float previousReloadSec = 0.0f;
	weapon_.GetReloadUI(wasReloading, previousReloadTimer, previousReloadSec);

	if (wasReloading)
	{
		SuppressActionInputDuringReload(ctx.rawSnap);
	}

	if (weaponVisual_.IsEquipAnimating())
	{
		SuppressActionInputDuringReload(ctx.rawSnap);
	}

	weaponController_.HandleWheelSwitch(ctx.rawSnap);

	weapon_.UpdateAndHandleInput(deltaTime, ctx.rawSnap);

	weapon_.GetReloadUI(
		ctx.reload.isReloading,
		ctx.reload.reloadTimer,
		ctx.reload.reloadSec);

	if (ctx.reload.isReloading)
	{
		SuppressActionInputDuringReload(ctx.rawSnap);
	}

	view_.SetReloadViewModelState(
		ctx.reload.isReloading,
		ctx.reload.reloadTimer,
		ctx.reload.reloadSec);

	weaponVisual_.SetReloadViewModelState(
		ctx.reload.isReloading,
		ctx.reload.reloadTimer,
		ctx.reload.reloadSec);
	view_.SetEquipViewModelState(
		weaponVisual_.IsEquipAnimating(),
		weaponVisual_.GetCurrentEquipOffset(),
		weaponVisual_.GetCurrentEquipPitchRad());

	if (weaponVisual_.IsEquipAnimating())
	{
		// 装備アニメーション中は攻撃入力を無効化する。
		SuppressActionInputDuringReload(ctx.rawSnap);
	}
}

void Player::FinalizeInputSnapshotForGameplay(float deltaTime, InputFrameContext& ctx)
{
	inputSnap_ = ctx.rawSnap;

	if (ctx.reload.isReloading)
	{
		SuppressActionInputDuringReload(inputSnap_);
	}

	auto* tr = GetWorldTransform();
	if (!tr)
	{
		return;
	}

	motor_.BindTransform(tr);
	motor_.PreprocessInput(inputSnap_, deltaTime);
	SetCenterPosition(tr->translate_);

	view_.BindBodyTransform(tr);
	view_.SetAiming(inputSnap_.aimHeld);
	view_.UpdateLook(deltaTime, inputSnap_);
}

void Player::ApplyWeaponCameraAndMovementTuning()
{
	float adsFovDeg = 60.0f;
	float adsSpeed = 10.0f;
	if (weapon_.GetCurrentAdsViewTuning(adsFovDeg, adsSpeed))
	{
		view_.SetWeaponAdsTuning(adsFovDeg, adsSpeed);
	}
	else
	{
		view_.SetWeaponAdsTuning(adsFovDeg, adsSpeed);
	}

	float adsMoveMul = 0.85f;
	if (weapon_.GetCurrentAdsMoveMultiplier(adsMoveMul))
	{
		motor_.SetAdsMoveMultiplier(adsMoveMul);
	}
	else
	{
		motor_.SetAdsMoveMultiplier(adsMoveMul);
	}
}

void Player::UpdateBrain(float deltaTime)
{
	brainComponent_.Update(inputSnap_, deltaTime);
}

void Player::UpdateMovementAndView(float deltaTime)
{
	const MovementContext ctx = BuildMovementContext();

	UpdateRunCarry(ctx);
	UpdateHudFromMovement(ctx);
	SimulateMovement(ctx, deltaTime);
	SyncViewAfterMovement(ctx, deltaTime);

	brainComponent_.SetPrevLocoId(ctx.cur);

	BaseCharacter::Update(deltaTime);
}

Player::MovementContext Player::BuildMovementContext() const
{
	MovementContext ctx{};

	ctx.prev = brainComponent_.GetPrevLocoId();
	ctx.cur = brainComponent_.GetCurrentLocoId();
	ctx.combat = brainComponent_.GetCurrentCombatId();

	ctx.isAds = (ctx.combat == CombatId::Aim) || inputSnap_.aimHeld;
	ctx.blinkJustStarted = (ctx.cur == LocoId::Blink && ctx.prev != LocoId::Blink);
	ctx.moving = IsMovingInput(inputSnap_);
	ctx.isAirLike = IsAirLikeLoco(ctx.cur);
	ctx.isBlinking = (ctx.cur == LocoId::Blink);

	float dummyReloadTimer = 0.0f;
	float dummyReloadSec = 0.0f;
	weapon_.GetReloadUI(
		ctx.isReloading,
		dummyReloadTimer,
		dummyReloadSec);

	return ctx;
}

void Player::UpdateRunCarry(const MovementContext& ctx)
{
	if (ctx.cur == LocoId::Run)
	{
		runtime_.runCarry = true;
	}

	if (ctx.prev == LocoId::Run && ctx.isAirLike)
	{
		runtime_.runCarry = true;
	}

	if (ctx.cur == LocoId::Blink)
	{
		runtime_.runCarry = false;
	}

	if (ctx.isAirLike)
	{
		if (!inputSnap_.sprintHeld || ctx.isAds || !ctx.moving)
		{
			runtime_.runCarry = false;
		}
	}
	else
	{
		if (ctx.cur != LocoId::Run)
		{
			runtime_.runCarry = false;
		}
	}
}

void Player::UpdateHudFromMovement(const MovementContext& ctx)
{
	if (!refs_.hudManager)
	{
		return;
	}

	const bool crosshairMoving = ctx.moving || ctx.isBlinking;
	const bool crosshairSprinting =
		(ctx.cur == LocoId::Run) ||
		(ctx.cur == LocoId::Blink) ||
		((ctx.cur == LocoId::Jump || ctx.cur == LocoId::Fall) && inputSnap_.sprintHeld && !ctx.isAds);
	const bool crosshairAirborne = (ctx.cur == LocoId::Jump || ctx.cur == LocoId::Fall);

	refs_.hudManager->SetCrosshairMovementState(
		crosshairMoving,
		crosshairSprinting,
		crosshairAirborne);

	if (ctx.cur == LocoId::Land && ctx.prev != LocoId::Land)
	{
		refs_.hudManager->NotifyCrosshairLanded();
	}

	refs_.hudManager->SetHP(damage_.GetHP(), damage_.GetMaxHP());
}

void Player::SimulateMovement(const MovementContext& ctx, float deltaTime)
{
	const bool isRunningForFov = (ctx.cur == LocoId::Run) || (ctx.isAirLike && runtime_.runCarry);

	motor_.Simulate(
		deltaTime,
		view_.GetYaw(),
		ctx.cur,
		ctx.isAds,
		ctx.isReloading);

	view_.UpdateMovementFov(
		deltaTime,
		isRunningForFov,
		ctx.isBlinking,
		ctx.blinkJustStarted);
}

void Player::SyncViewAfterMovement(const MovementContext& /*ctx*/, float /*deltaTime*/)
{
	view_.SyncToPlayer();
	view_.SyncViewModeToFirstPersonFlag();
}

void Player::UpdatePresentation(float deltaTime)
{
	view_.SetFirstPersonLeftArmVisible(!IsCurrentWeaponMeleeForView());

	weaponVisual_.Update(deltaTime, inputSnap_.aimHeld);
	SyncHurtboxes();
	vfx_.Update(deltaTime);
}

void Player::SyncHurtboxes()
{
	hurtbox_.Sync(*this);
}

/// -------------------------------------------------------------
///					　			 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	BaseCharacter::Draw();
	weaponVisual_.Draw();
}

/// -------------------------------------------------------------
///				　			ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{
#ifdef USE_IMGUI
	// 互換用の一括描画は新しい用途別Debugパネルの中身を再利用する。
	DrawPlayerDebugImGui();
	DrawWeaponDebugImGui();
#endif
}

void Player::DrawPlayerDebugImGui()
{
#ifdef USE_IMGUI
	// Player Debugには腕・照準・被弾判定・HP系の調整を集約する。
	ImGui::Text("HP: %.1f", GetHP());
	view_.DrawImGui();
	combat_.DrawImGui();
	hurtbox_.DrawImGui();
#endif
}

void Player::DrawWeaponDebugImGui()
{
#ifdef USE_IMGUI
	// Weapon Debugには武器状態と見た目調整を集約する。
	weapon_.DrawImGui();
	weaponVisual_.DrawImGui();
#endif
}

void Player::DrawShadow()
{
	BaseCharacter::DrawShadow();
	weaponVisual_.DrawShadow();
}

void Player::UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
{
	BaseCharacter::UpdateShadowMatrix(lightViewProjection);
}

/// -------------------------------------------------------------
///				　			衝突判定処理
/// -------------------------------------------------------------
void Player::OnCollision(K4E::Collider* other)
{
	if (!other) return;

	OnHitByEnemyBullet(other, PlayerHitPart::Body, 1.0f);
}

void Player::OnHitByEnemyBullet(K4E::Collider* bullet, PlayerHitPart part, float mul)
{
	const auto fb = damage_.OnHitByEnemyBullet(
		*this,
		bullet,
		part,
		mul,
		view_,
		weaponController_,
		death_,
		inputSnap_,
		api_,
		runtime_.runCarry,
		audio_.onHit,
		audio_.onDeath);

	ApplyDamageFeedback(fb);

	if (!fb.tookDamage || !refs_.hudManager || !bullet)
	{
		return;
	}

	auto* cam = view_.GetCamera();
	if (!cam)
	{
		return;
	}

	K4E::Vector3 attackerPos = bullet->GetCenterPosition();

	if (auto* bulletObj = bullet->GetOwner<Bullet>())
	{
		attackerPos = bulletObj->GetShooterPosition();
	}

	K4E::Vector3 playerPos = GetWorldTransform()->translate_;

	K4E::Vector3 cameraForward = cam->GetForward();
	cameraForward.y = 0.0f;

	const float forwardLenSq =
		cameraForward.x * cameraForward.x +
		cameraForward.z * cameraForward.z;

	if (forwardLenSq <= 0.0001f)
	{
		cameraForward = { 0.0f, 0.0f, 1.0f };
	}
	else
	{
		cameraForward = K4E::Vector3::Normalize(cameraForward);
	}

	K4E::Vector3 cameraRight =
	{
		cameraForward.z,
		0.0f,
		-cameraForward.x
	};
	cameraRight = K4E::Vector3::Normalize(cameraRight);

	refs_.hudManager->AddDamageIndicator(
		playerPos,
		attackerPos,
		cameraForward,
		cameraRight);
}

void Player::NotifyEnemyHitUI(bool isHeadshot)
{
	if (refs_.hudManager)
	{
		refs_.hudManager->NotifyEnemyHit(isHeadshot);
	}
}

void Player::NotifyEnemyKillUI(bool isHeadshot)
{
	if (refs_.hudManager)
	{
		refs_.hudManager->NotifyEnemyKill(isHeadshot);
	}
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

void Player::SetSpawnPosition(const K4E::Vector3& worldPos)
{
	spawnPos_ = worldPos;
	hasSpawnPos_ = true;

	if (auto* tr = GetWorldTransform())
	{
		tr->translate_ = spawnPos_ + spawnOffset_;
		SetCenterPosition(tr->translate_);
	}
}

void Player::SetSpawnOffset(const K4E::Vector3& offset)
{
	const K4E::Vector3 delta = offset - spawnOffset_;
	spawnOffset_ = offset;

	if (auto* tr = GetWorldTransform())
	{
		if (hasSpawnPos_) tr->translate_ = spawnPos_ + spawnOffset_;
		else              tr->translate_ += delta;

		SetCenterPosition(tr->translate_);
	}
}

void Player::SetViewLookAngles(float pitchRad, float yawRad)
{
	view_.SetLookAngles(pitchRad, yawRad);
}

void Player::SyncViewToPlayer()
{
	view_.SyncToPlayer();
	view_.SyncViewModeToFirstPersonFlag();
}

void Player::SetStartGameplayVisualsVisible(bool visible)
{
	weaponVisual_.SetVisible(visible);
}

void Player::WarmupStartGameplayVisuals()
{
	// カメラ切り替え時の一括初期化を避けるため、FPS腕表示と武器モデルを事前構築しておく。
	view_.SetFirstPersonView(true);
	weaponVisual_.SetVisible(false);
	weaponVisual_.Update(0.0f, false);
	SyncHurtboxes();
}

void Player::ApplyEditedWeaponDataFromEditor(int32_t weaponID, const FWeaponMasterData& data)
{
	auto& db = weapon_.GetWeaponMasterDatabase();
	FWeaponMasterData* runtimeData = db.FindMutableByID(weaponID);
	if (!runtimeData)
	{
		return;
	}

	*runtimeData = data;

	if (weapon_.GetCurrentWeaponId() == weaponID)
	{
		weapon_.RebuildCurrentWeaponFromDatabase();
		weaponVisual_.ForceRefresh();
	}
}

void Player::ApplyFallDamage(float deltaTime)
{
	const auto fb = damage_.ApplyFallDamage(
		*this,
		deltaTime,
		fallDamageSettings_,
		view_,
		weaponController_,
		death_,
		inputSnap_,
		api_,
		runtime_.runCarry,
		audio_.onDeath);

	ApplyDamageFeedback(fb);
}

void Player::StartDeath(const K4E::Vector3& launchDirWorld)
{
	death_.Start(
		*this,
		launchDirWorld,
		view_,
		weaponVisual_,
		inputSnap_,
		runtime_.runCarry,
		[this]() { weaponController_.CancelReloadOnly(); });
}

void Player::UpdateDeath(float deltaTime)
{
	death_.Update(
		*this,
		deltaTime,
		motor_,
		view_,
		weaponVisual_,
		hurtbox_,
		vfx_,
		refs_.hudManager,
		damage_.GetHP(),
		damage_.GetMaxHP());
}

void Player::ApplyDamageFeedback(const PlayerDamageComponent::DamageFeedback& fb)
{
	if (!fb.tookDamage)
	{
		return;
	}

	vfx_.OnDamaged(fb.damage, fb.maxHp);

	if (onDamageTaken_)
	{
		onDamageTaken_();
	}

	if (fb.notifyPlayerHit)
	{
		if (auto* tr = GetWorldTransform())
		{
			K4E::Vector3 emitPos = tr->translate_;
			emitPos.y += 1.0f;

			K4E::GpuParticleManager::GetInstance()->EmitBurst(
				"PlayerDamageBlood",
				K4E::GpuParticleType::PlayerDamageBlood,
				emitPos,
				40);
		}
	}

	if (refs_.hudManager)
	{
		if (fb.hpChanged)
		{
			refs_.hudManager->SetHP(fb.hpAfter, fb.maxHp);
		}

		if (fb.notifyPlayerHit)
		{
			refs_.hudManager->NotifyPlayerHit(fb.hitStrength01);
		}
	}
}

bool Player::IsCurrentWeaponMeleeForView() const
{
	// 例: 近接武器がスロット2に入っている場合
	return weapon_.GetSelectedHot_barIndex() == 2;
}
