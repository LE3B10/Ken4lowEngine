#define NOMINMAX
#include "PlayerViewComponent.h"
#include "LinearInterpolation.h"

#include <cmath>
#include <numbers>


using namespace Ken4lowEngine;

static inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

static float Approach(float current, float target, float speed, float deltaTime)
{
	const float step = speed * deltaTime;
	const float diff = target - current;
	if (diff > step) return current + step;
	if (diff < -step) return current - step;
	return target;
}

void PlayerViewComponent::BindArmTransforms(K4E::WorldTransformEx* leftArm, K4E::WorldTransformEx* rightArm)
{
	leftArmTr_ = leftArm;
	rightArmTr_ = rightArm;

	// ベース（初期）ポーズをキャプチャ
	if (!capturedBasePose_ && leftArmTr_ && rightArmTr_)
	{
		baseLeftPos_ = leftArmTr_->translate_;
		baseRightPos_ = rightArmTr_->translate_;
		/*baseLeftRot_ = leftArmTr_->rotate_;
		baseRightRot_ = rightArmTr_->rotate_;*/
		capturedBasePose_ = true;
	}
}

void PlayerViewComponent::SetEmptyAimPose(const K4E::Vector3& leftLocalPos, const K4E::Vector3& rightLocalPos)
{
	aimLeftPos_ = leftLocalPos;
	aimRightPos_ = rightLocalPos;
}

void PlayerViewComponent::Initialize(Player* player)
{
	BindOwner(player);

	// FpsCamera は “プレイヤーに追従” する前提で Initialize を要求する
	fpsCamera_.Initialize(player);

	// 既定の shoot camera は fpsCamera の camera
	shootCamera_ = fpsCamera_.GetCamera();

	// 初期の ViewMode を反映
	SyncViewModeToFirstPersonFlag();

	// 初期は腕をリラックス
	armBlend_ = 0.0f;
	isAiming_ = false;
}

void PlayerViewComponent::SetAiming(bool on)
{
	isAiming_ = on;
	fpsCamera_.SetAiming(on);
}

void PlayerViewComponent::UpdateLook(float dt, const InputSnapshot& input)
{
	fpsCamera_.SetDeltaTime(dt);
	fpsCamera_.UpdateLook(input);

	// 体のYawをカメラYawに合わせる（移動の基準にもなる）
	if (bodyTr_) bodyTr_->rotate_.y = fpsCamera_.GetYaw();

	// cache pitch for arms
	camPitch_ = fpsCamera_.GetPitch();

	// 腕の構え（見た目）更新
	UpdateFirstPersonArmPose(dt);
}

void PlayerViewComponent::SyncToPlayer()
{
	fpsCamera_.SyncToPlayer();
}

void PlayerViewComponent::SyncViewModeToFirstPersonFlag()
{
	const bool fp = (fpsCamera_.GetViewMode() == K4E::FpsCamera::ViewMode::FirstPerson);
	SetFirstPersonView(fp);
}

void PlayerViewComponent::SetFirstPersonView(bool enabled)
{
	if (isFirstPersonView_ == enabled) return;
	isFirstPersonView_ = enabled;

	// 表示切替
	ApplyFirstPersonRenderFlags();

	// 3人称へ戻した時に腕が変な位置のまま残らないように即リセット
	if (!isFirstPersonView_ && capturedBasePose_ && leftArmTr_ && rightArmTr_)
	{
		leftArmTr_->translate_ = baseLeftPos_;
		rightArmTr_->translate_ = baseRightPos_;
		leftArmTr_->rotate_ = baseLeftRot_;
		rightArmTr_->rotate_ = baseRightRot_;
		armBlend_ = 0.0f;
	}
}

void PlayerViewComponent::UpdateMovementFov(float deltaTime, bool isRunning, bool isBlinking, bool blinkJustStarted)
{
	// 一人称だけに効かせる
	if (fpsCamera_.GetViewMode() != K4E::FpsCamera::ViewMode::FirstPerson) return;

	// カメラ取得（fovHooks優先、なければfpsCamera）
	auto* cam = fpsCamera_.GetCamera();
	if (!cam) return;

	auto getFovRad = [&]() -> float
		{
			if (fovHooks_.GetFov) return fovHooks_.GetFov();
			return cam->GetFovY();
		};

	auto setFovRad = [&](float fovRad)
		{
			if (fovHooks_.SetFov) fovHooks_.SetFov(fovRad);
			else cam->SetFovY(fovRad);
		};

	// 現在FOV（度）
	const float currentDeg = RadToDeg(getFovRad());

	// 腰だめ基準FOVを初回だけキャプチャ
	if (!hipBaseFovCaptured_)
	{
		hipBaseFovDeg_ = std::clamp(currentDeg, minFovDeg_, maxFovDeg_);
		hipBaseFovCaptured_ = true;
	}

	// ADSしていない & 走行/ダッシュ演出が乗っていない時は、
	// 外部変更（デバッグ等）を腰だめ基準として取り直せるようにする
	const bool noMoveFovEffect =
		(std::abs(runAlpha_) < 0.0001f) &&
		(std::abs(blinkAlpha_) < 0.0001f) &&
		(std::abs(blinkKick_) < 0.0001f);

	if (!isAiming_ && noMoveFovEffect)
	{
		hipBaseFovDeg_ = std::clamp(currentDeg, minFovDeg_, maxFovDeg_);
	}

	// ---- 武器ADS FOVの補間（武器データの速度を使う）----
	const float adsTarget = isAiming_ ? 1.0f : 0.0f;
	const float adsSpeed = std::max(0.01f, weaponAdsTransitionSpeed_);
	adsFovAlpha_ = Approach(adsFovAlpha_, adsTarget, adsSpeed, deltaTime);

	// 腰だめFOV -> 武器ADS FOVへ補間
	const float adsBaseDeg =
		hipBaseFovDeg_ + (weaponAdsFovDeg_ - hipBaseFovDeg_) * adsFovAlpha_;

	// ---- 走行/ダッシュFOV演出（ADS中は弱める）----
	runAlpha_ = Approach(runAlpha_, isRunning ? 1.0f : 0.0f, isRunning ? runInSpeed_ : runOutSpeed_, deltaTime);
	blinkAlpha_ = Approach(blinkAlpha_, isBlinking ? 1.0f : 0.0f, isBlinking ? blinkInSpeed_ : blinkOutSpeed_, deltaTime);

	if (blinkJustStarted) blinkKick_ = 1.0f;
	blinkKick_ = Approach(blinkKick_, 0.0f, blinkKickOutSpeed_, deltaTime);

	float suppressScale = 1.0f - adsFovAlpha_ * adsSuppress_;
	suppressScale = std::clamp(suppressScale, 0.0f, 1.0f);

	const float moveAddDeg =
		(runAlpha_ * runFovAddDeg_ +
			blinkAlpha_ * blinkFovAddDeg_ +
			blinkKick_ * blinkKickAddDeg_) * suppressScale;

	dmgFovKickDeg_ = Approach(dmgFovKickDeg_, 0.0f, dmgFovReturnSpeed_, deltaTime);

	const float outDeg = std::clamp(adsBaseDeg + moveAddDeg + dmgFovKickDeg_, minFovDeg_, maxFovDeg_);

	setFovRad(DegToRad(outDeg));
}

void PlayerViewComponent::AddRecoil(float verticalDeg, float horizontalDeg)
{
	// 1) ゲームプレイ反動（照準がズレる）= カメラ反動
	fpsCamera_.AddRecoil(DegToRad(verticalDeg), DegToRad(horizontalDeg));

	// 2) 見た目反動（撃った感）= 一人称の腕/武器のキック
	const float sideSign = vmKickFlip_ ? 1.0f : -1.0f;
	vmKickFlip_ = !vmKickFlip_;

	vmKickPitch_ += DegToRad(verticalDeg * vmKickPitchMul_);
	vmKickYaw_ += DegToRad(horizontalDeg * vmKickYawMul_) * sideSign;
	vmKickRoll_ += DegToRad(horizontalDeg * vmKickRollMul_) * sideSign;

	vmKickBack_ += verticalDeg * vmKickBackPerPitchDeg_;
	vmKickUp_ += verticalDeg * vmKickUpPerPitchDeg_;

	// Clamp（暴れすぎ防止）
	vmKickPitch_ = std::clamp(vmKickPitch_, -DegToRad(vmKickMaxPitchDeg_), DegToRad(vmKickMaxPitchDeg_));
	vmKickYaw_ = std::clamp(vmKickYaw_, -DegToRad(vmKickMaxYawDeg_), DegToRad(vmKickMaxYawDeg_));
	vmKickRoll_ = std::clamp(vmKickRoll_, -DegToRad(vmKickMaxRollDeg_), DegToRad(vmKickMaxRollDeg_));
	vmKickBack_ = std::clamp(vmKickBack_, 0.0f, vmKickMaxBack_);
	vmKickUp_ = std::clamp(vmKickUp_, 0.0f, vmKickMaxUp_);
}

void PlayerViewComponent::SetWeaponAdsTuning(float adsFovDeg, float adsTransitionSpeed)
{
	weaponAdsFovDeg_ = std::clamp(adsFovDeg, 1.0f, 179.0f);
	weaponAdsTransitionSpeed_ = std::max(0.01f, adsTransitionSpeed);
}

void PlayerViewComponent::AddDamageFeedback(float strength01)
{
	strength01 = Clamp01(strength01);
	if (strength01 <= 0.0f) return;

	// 左右に交互に振る（一定のランダムっぽさ）
	const float sideSign = dmgFlip_ ? 1.0f : -1.0f;
	dmgFlip_ = !dmgFlip_;

	// 1) カメラ（照準）をガクッとさせる：被弾の手応え
	// ※向きが逆に感じるなら pitch の符号を反転してOK
	fpsCamera_.AddRecoil(DegToRad(dmgCamPitchDeg_ * strength01),
		DegToRad(dmgCamYawDeg_ * strength01) * sideSign);

	// 2) FOVを一瞬だけ広げる（衝撃感）
	dmgFovKickDeg_ = std::clamp(dmgFovKickDeg_ + dmgFovKickAddDeg_ * strength01,
		0.0f, dmgFovKickMaxDeg_);

	// 3) 見た目（腕/武器）も少しだけ“ビクッ”（任意）
	vmKickRoll_ += DegToRad((dmgCamYawDeg_ * 0.90f) * strength01) * sideSign;
	vmKickBack_ += 0.020f * strength01;
	vmKickUp_ += 0.008f * strength01;

	// Clamp（暴れすぎ防止）
	vmKickRoll_ = std::clamp(vmKickRoll_, -DegToRad(vmKickMaxRollDeg_), DegToRad(vmKickMaxRollDeg_));
	vmKickBack_ = std::clamp(vmKickBack_, 0.0f, vmKickMaxBack_);
	vmKickUp_ = std::clamp(vmKickUp_, 0.0f, vmKickMaxUp_);
}

void PlayerViewComponent::SetFirstPersonLeftArmVisible(bool visible)
{
	if (fpShowLeftArm_ == visible) return;

	fpShowLeftArm_ = visible;

	if (isFirstPersonView_) ApplyFirstPersonRenderFlags();
}

void PlayerViewComponent::StartMeleeSwing()
{
	meleeSwingActive_ = true;
	meleeSwingTimer_ = 0.0f;
}

void PlayerViewComponent::StartDeathCamera(float targetPitchRad, float targetRollRad)
{
	deathCameraActive_ = true;
	deathCamPitchNow_ = 0.0f;
	deathCamRollNow_ = 0.0f;
	deathCamPitchTarget_ = targetPitchRad;
	deathCamRollTarget_ = targetRollRad;
}

void PlayerViewComponent::UpdateDeathCamera(float dt, float normalizedT)
{
	if (!deathCameraActive_) return;

	normalizedT = std::clamp(normalizedT, 0.0f, 1.0f);

	const float targetPitch = deathCamPitchTarget_ * normalizedT;
	const float targetRoll = deathCamRollTarget_ * normalizedT;

	const float speed = std::max(0.01f, deathCamBlendSpeed_);
	deathCamPitchNow_ = Approach(deathCamPitchNow_, targetPitch, speed, dt);
	deathCamRollNow_ = Approach(deathCamRollNow_, targetRoll, speed, dt);

	fpsCamera_.SetDeathTilt(deathCamPitchNow_, deathCamRollNow_);
}

void PlayerViewComponent::ClearDeathCamera()
{
	deathCameraActive_ = false;
	deathCamPitchNow_ = 0.0f;
	deathCamRollNow_ = 0.0f;
	deathCamPitchTarget_ = 0.0f;
	deathCamRollTarget_ = 0.0f;

	fpsCamera_.ClearDeathTilt();
}

void PlayerViewComponent::ApplyFirstPersonRenderFlags()
{
	if (!fpHooks_.SetBodyActive || !fpHooks_.SetAllPartsActive || !fpHooks_.SetPartActive
		|| !fpHooks_.GetLeftArmIndex || !fpHooks_.GetRightArmIndex)
	{
		return;
	}

	if (isFirstPersonView_)
	{
		fpHooks_.SetBodyActive(false);
		fpHooks_.SetAllPartsActive(false);

		const int l = fpHooks_.GetLeftArmIndex();
		const int r = fpHooks_.GetRightArmIndex();

		if (l >= 0) fpHooks_.SetPartActive(l, fpShowLeftArm_);
		if (r >= 0) fpHooks_.SetPartActive(r, true);
	}
	else
	{
		fpHooks_.SetBodyActive(true);
		fpHooks_.SetAllPartsActive(true);
	}
}

void PlayerViewComponent::UpdateRecoilViewModelKick(float dt)
{
	vmKickPitch_ = Approach(vmKickPitch_, 0.0f, vmKickRotReturnSpeed_, dt);
	vmKickYaw_ = Approach(vmKickYaw_, 0.0f, vmKickRotReturnSpeed_, dt);
	vmKickRoll_ = Approach(vmKickRoll_, 0.0f, vmKickRotReturnSpeed_, dt);

	vmKickBack_ = Approach(vmKickBack_, 0.0f, vmKickPosReturnSpeed_, dt);
	vmKickUp_ = Approach(vmKickUp_, 0.0f, vmKickPosReturnSpeed_, dt);
}

void PlayerViewComponent::UpdateFirstPersonArmPose(float dt)
{
	if (!isFirstPersonView_ || !capturedBasePose_ || !leftArmTr_ || !rightArmTr_) return;

	const float target = isAiming_ ? 1.0f : 0.0f;

	const float maxDelta = armBlendSpeed_ * dt;
	if (armBlend_ < target) armBlend_ = (std::min)(target, armBlend_ + maxDelta);
	else if (armBlend_ > target) armBlend_ = (std::max)(target, armBlend_ - maxDelta);

	const float t = Clamp01(armBlend_);

	UpdateRecoilViewModelKick(dt);

	leftArmTr_->translate_ = baseLeftPos_;
	rightArmTr_->translate_ = baseRightPos_;

	leftArmTr_->translate_.z -= vmKickBack_ * 0.90f;
	rightArmTr_->translate_.z -= vmKickBack_ * 1.05f;
	leftArmTr_->translate_.y += vmKickUp_ * 0.90f;
	rightArmTr_->translate_.y += vmKickUp_ * 1.00f;
	leftArmTr_->translate_.x -= vmKickYaw_ * 0.010f;
	rightArmTr_->translate_.x -= vmKickYaw_ * 0.018f;

	K4E::Vector3 lrot = Lerp(baseLeftRot_, aimLeftRot_, t);
	K4E::Vector3 rrot = Lerp(baseRightRot_, aimRightRot_, t);

	// カメラのピッチ追従
	lrot.x += camPitch_ * armPitchFollow_;
	rrot.x += camPitch_ * armPitchFollow_;

	// 通常の見た目反動
	lrot.x += vmKickPitch_ * 0.75f;
	lrot.y += vmKickYaw_ * 0.35f;
	lrot.z += vmKickRoll_ * 0.60f;

	rrot.x += vmKickPitch_;
	rrot.y += vmKickYaw_;
	rrot.z += vmKickRoll_;

	// -----------------------------
	// 近接スイング
	// -----------------------------
	if (meleeSwingActive_)
	{
		meleeSwingTimer_ += dt;
		float nt = meleeSwingTimer_ / meleeSwingDuration_;
		nt = std::clamp(nt, 0.0f, 1.0f);

		// 0.0-0.2: 溜め
		// 0.2-0.65: 振り
		// 0.65-1.0: 戻し
		float windup = 0.0f;
		float slash = 0.0f;
		float recover = 0.0f;

		if (nt < 0.2f)
		{
			windup = nt / 0.2f;
		}
		else if (nt < 0.65f)
		{
			slash = (nt - 0.2f) / 0.45f;
		}
		else
		{
			recover = (nt - 0.65f) / 0.35f;
		}

		// 溜め
		if (windup > 0.0f)
		{
			rrot.x += DegToRad(18.0f) * windup;
			rrot.y += DegToRad(10.0f) * windup;
			rrot.z += DegToRad(12.0f) * windup;

			rightArmTr_->translate_.x += 0.04f * windup;
			rightArmTr_->translate_.y += 0.02f * windup;
			rightArmTr_->translate_.z -= 0.03f * windup;
		}

		// 振り
		if (slash > 0.0f)
		{
			const float s = std::sin(slash * std::numbers::pi_v<float>);

			rrot.x -= DegToRad(meleeSwingPitchDeg_) * s;
			rrot.y -= DegToRad(meleeSwingYawDeg_) * s;
			rrot.z -= DegToRad(meleeSwingRollDeg_) * s;

			rightArmTr_->translate_.x += meleeSwingRight_ * s;
			rightArmTr_->translate_.y -= meleeSwingDown_ * s;
			rightArmTr_->translate_.z += meleeSwingForward_ * s;
		}

		// 戻し
		if (recover > 0.0f)
		{
			// 特別なことはせず、通常姿勢へ戻る補間に任せる
		}

		if (nt >= 1.0f)
		{
			meleeSwingActive_ = false;
			meleeSwingTimer_ = 0.0f;
		}
	}

	leftArmTr_->rotate_ = lrot;
	rightArmTr_->rotate_ = rrot;
}
