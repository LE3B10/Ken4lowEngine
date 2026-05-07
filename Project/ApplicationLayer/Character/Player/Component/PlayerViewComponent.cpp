#define NOMINMAX
#include "PlayerViewComponent.h"
#include "LinearInterpolation.h"

#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI


using namespace Ken4lowEngine;

static inline float Clamp01(float v) { return (v < 0.0f) ? 0.0f : (v > 1.0f ? 1.0f : v); }

static float SmoothStep01(float t)
{
	t = Clamp01(t);
	return t * t * (3.0f - 2.0f * t);
}

static float Pulse01(float progress, float start, float peak, float end)
{
	progress = Clamp01(progress);
	start = std::clamp(start, 0.0f, 0.98f);
	peak = std::clamp(peak, start + 0.01f, 0.99f);
	end = std::clamp(end, peak + 0.01f, 1.0f);

	if (progress < start || progress >= end)
	{
		return 0.0f;
	}

	if (progress < peak)
	{
		return SmoothStep01((progress - start) / (peak - start));
	}

	return 1.0f - SmoothStep01((progress - peak) / (end - peak));
}

static float Approach(float current, float target, float speed, float deltaTime)
{
	const float step = speed * deltaTime;
	const float diff = target - current;
	if (diff > step) return current + step;
	if (diff < -step) return current - step;
	return target;
}

static float CalcReloadPoseWeight(float progress, float enterEnd, float holdEnd, float returnEnd)
{
	progress = Clamp01(progress);
	enterEnd = std::clamp(enterEnd, 0.01f, 0.95f);
	holdEnd = std::clamp(holdEnd, enterEnd + 0.01f, 0.98f);
	returnEnd = std::clamp(returnEnd, holdEnd + 0.01f, 1.0f);

	if (progress < enterEnd)
	{
		return SmoothStep01(progress / enterEnd);
	}

	if (progress < holdEnd)
	{
		return 1.0f;
	}

	if (progress < returnEnd)
	{
		const float t = (progress - holdEnd) / (returnEnd - holdEnd);
		return 1.0f - SmoothStep01(t);
	}

	return 0.0f;
}

static K4E::Vector3 RotatePitchAroundPivot(const K4E::Vector3& pos, const K4E::Vector3& pivot, float pitch)
{
	K4E::Vector3 local = pos - pivot;

	const float s = std::sin(pitch);
	const float c = std::cos(pitch);

	K4E::Vector3 rotated = local;
	rotated.y = local.y * c - local.z * s;
	rotated.z = local.y * s + local.z * c;

	return rotated + pivot;
}

static Quaternion MakeQuaternionFromEulerRad(const Vector3& eulerRad)
{
	const Quaternion qx = Quaternion::MakeRotateAxisAngleQuaternion({ 1.0f, 0.0f, 0.0f }, eulerRad.x);
	const Quaternion qy = Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 1.0f, 0.0f }, eulerRad.y);
	const Quaternion qz = Quaternion::MakeRotateAxisAngleQuaternion({ 0.0f, 0.0f, 1.0f }, eulerRad.z);

	return Quaternion::Normalize(Quaternion::Multiply(Quaternion::Multiply(qx, qy), qz));
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

void PlayerViewComponent::SetReloadViewModelState(bool isReloading, float reloadTimer, float reloadDuration)
{
	const bool wasReloading = reloadViewActive_;

	reloadViewActive_ = isReloading;
	reloadViewTimer_ = reloadTimer;
	reloadViewDuration_ = std::max(0.01f, reloadDuration);

	if (isReloading)
	{
		// reloadTimer は残り時間として渡ってくる可能性があるため、表示用には自前タイマーで進行度を作る。
		if (!wasReloading)
		{
			reloadAnimTimer_ = 0.0f;
		}
	}
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
		leftArmTr_->useQuaternionRotation_ = false;
		rightArmTr_->useQuaternionRotation_ = false;
		armBlend_ = 0.0f;
		reloadPoseAlpha_ = 0.0f;
		reloadAnimTimer_ = 0.0f;
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

void PlayerViewComponent::DrawImGui()
{
	fpsCamera_.DrawImGui();

#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("ViewModel Arms", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Left Arm normal / ADS pose");
		ImGui::DragFloat3("Left Hip Support Offset", &leftHipSupportOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Base Left Rot", &baseLeftRot_.x, 0.01f, -6.28f, 6.28f, "%.2f");
		ImGui::DragFloat3("Aim Left Pos", &aimLeftPos_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Aim Left Rot", &aimLeftRot_.x, 0.01f, -6.28f, 6.28f, "%.2f");
		ImGui::Separator();

		ImGui::Text("Right Arm ADS pose");
		ImGui::DragFloat3("Right Hip Support Offset", &rightHipSupportOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Base Right Rot", &baseRightRot_.x, 0.01f, -6.28f, 6.28f, "%.2f");
		ImGui::DragFloat3("Aim Right Pos", &aimRightPos_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Aim Right Rot", &aimRightRot_.x, 0.01f, -6.28f, 6.28f, "%.2f");
		ImGui::Separator();
		ImGui::Text("Pitch ViewModel Offset");
		ImGui::DragFloat("Pitch Offset Max Deg", &viewModelPitchOffsetMaxDeg_, 1.0f, 10.0f, 89.0f, "%.1f");
		ImGui::DragFloat3("Pitch Up Left Offset", &pitchUpLeftArmOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Pitch Down Left Offset", &pitchDownLeftArmOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Pitch Up Right Offset", &pitchUpRightArmOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::DragFloat3("Pitch Down Right Offset", &pitchDownRightArmOffset_.x, 0.01f, -3.0f, 3.0f, "%.2f");
		ImGui::Separator();
		ImGui::Text("Reload Alpha: %.2f", reloadPoseAlpha_);
		ImGui::Text("Reload Progress: %.2f", Clamp01(reloadAnimTimer_ / std::max(0.01f, reloadViewDuration_)));
		ImGui::Text("Weapon rotation follows Right Arm during reload.");
		ImGui::DragFloat3("Reload Right Arm Offset", &reloadWeaponDrop_.x, 0.01f, -2.0f, 2.0f, "%.2f");
		ImGui::DragFloat3("Reload Right Arm Rot Deg", &reloadRightArmRotDeg_.x, 0.25f, -180.0f, 180.0f, "%.2f");
		ImGui::Separator();
		ImGui::Text("Loading motion");
		ImGui::DragFloat("Load Start", &reloadLoadStartRate_, 0.01f, 0.20f, 0.80f, "%.2f");
		ImGui::DragFloat("Load Peak", &reloadLoadPeakRate_, 0.01f, 0.25f, 0.90f, "%.2f");
		ImGui::DragFloat("Load End", &reloadLoadEndRate_, 0.01f, 0.30f, 0.95f, "%.2f");
		ImGui::DragFloat3("Load Right Offset", &reloadLoadRightArmOffset_.x, 0.01f, -1.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Load Right Rot Deg", &reloadLoadRightArmRotDeg_.x, 0.25f, -90.0f, 90.0f, "%.2f");
		ImGui::Separator();
		ImGui::Text("Left Arm reload pose");
		ImGui::DragFloat3("Reload Left Arm Offset", &reloadLeftArmOffset_.x, 0.01f, -2.0f, 2.0f, "%.2f");
		ImGui::DragFloat3("Reload Left Arm Rot Deg", &reloadLeftArmRotDeg_.x, 0.25f, -180.0f, 180.0f, "%.2f");
		ImGui::DragFloat3("Load Left Offset", &reloadLoadLeftArmOffset_.x, 0.01f, -1.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Load Left Rot Deg", &reloadLoadLeftArmRotDeg_.x, 0.25f, -90.0f, 90.0f, "%.2f");
		ImGui::Separator();
		ImGui::DragFloat("Reload Enter End", &reloadEnterEndRate_, 0.01f, 0.05f, 0.60f, "%.2f");
		ImGui::DragFloat("Reload Hold End", &reloadHoldEndRate_, 0.01f, 0.20f, 0.95f, "%.2f");
		ImGui::DragFloat("Reload Return End", &reloadReturnEndRate_, 0.01f, 0.40f, 1.00f, "%.2f");
		ImGui::DragFloat("Reload Pose Blend Speed", &reloadPoseBlendSpeed_, 0.1f, 1.0f, 40.0f, "%.1f");
	}
#endif
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

	if (reloadViewActive_)
	{
		reloadAnimTimer_ = std::min(reloadAnimTimer_ + dt, reloadViewDuration_);
	}
	else if (reloadPoseAlpha_ <= 0.0f)
	{
		reloadAnimTimer_ = 0.0f;
	}

	const float reloadProgress = Clamp01(reloadAnimTimer_ / std::max(0.01f, reloadViewDuration_));
	const float timedReloadTarget = reloadViewActive_
		? CalcReloadPoseWeight(reloadProgress, reloadEnterEndRate_, reloadHoldEndRate_, reloadReturnEndRate_)
		: 0.0f;
	const float loadT = Pulse01(reloadProgress, reloadLoadStartRate_, reloadLoadPeakRate_, reloadLoadEndRate_) * timedReloadTarget;

	reloadPoseAlpha_ = Approach(reloadPoseAlpha_, timedReloadTarget, reloadPoseBlendSpeed_, dt);

	const float t = Clamp01(armBlend_);
	const float reloadT = SmoothStep01(reloadPoseAlpha_);
	const float loadSmoothT = SmoothStep01(loadT);

	UpdateRecoilViewModelKick(dt);

	// 左腕は通常時から少し武器側へ寄せる。
	// ここで位置も補間しないと、回転だけ変えても腕が画面外側に残りやすい。
	const float pitchRangeRad = DegToRad(std::max(1.0f, viewModelPitchOffsetMaxDeg_));

	// camPitch_ が + なら上向き、- なら下向き想定。
	// もし実機で逆に動いたら、camPitch_ と -camPitch_ を入れ替える。
	const float upT = SmoothStep01(std::clamp(camPitch_ / pitchRangeRad, 0.0f, 1.0f));
	const float downT = SmoothStep01(std::clamp(-camPitch_ / pitchRangeRad, 0.0f, 1.0f));

	const K4E::Vector3 pitchLeftOffset =
		pitchUpLeftArmOffset_ * upT +
		pitchDownLeftArmOffset_ * downT;

	const K4E::Vector3 pitchRightOffset =
		pitchUpRightArmOffset_ * upT +
		pitchDownRightArmOffset_ * downT;

	const K4E::Vector3 leftHipPos =
		baseLeftPos_ +
		leftHipSupportOffset_ +
		pitchLeftOffset;

	const K4E::Vector3 rightHipPos =
		baseRightPos_ +
		rightHipSupportOffset_ +
		pitchRightOffset;

	leftArmTr_->translate_ = Lerp(leftHipPos, aimLeftPos_ + pitchLeftOffset, t);
	rightArmTr_->translate_ = Lerp(rightHipPos, aimRightPos_ + pitchRightOffset, t);

	// 位置はPitch回転で回さない。
	// 上下視点時の画面内維持は pitchLeftOffset / pitchRightOffset で行う。
	//const float viewPitch = camPitch_ * armPitchFollow_;

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

	// リロード姿勢へ入る → 装填する押し込み → 戻る。
	if (reloadT > 0.0f)
	{
		rightArmTr_->translate_ += reloadWeaponDrop_ * reloadT;
		leftArmTr_->translate_ += reloadLeftArmOffset_ * reloadT;
	}

	if (loadSmoothT > 0.0f)
	{
		rightArmTr_->translate_ += reloadLoadRightArmOffset_ * loadSmoothT;
		leftArmTr_->translate_ += reloadLoadLeftArmOffset_ * loadSmoothT;
	}

	// -----------------------------
	// 近接スイング
	// -----------------------------
	if (meleeSwingActive_)
	{
		meleeSwingTimer_ += dt;
		float nt = meleeSwingTimer_ / meleeSwingDuration_;
		nt = std::clamp(nt, 0.0f, 1.0f);

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

		if (windup > 0.0f)
		{
			rrot.x += DegToRad(18.0f) * windup;
			rrot.y += DegToRad(10.0f) * windup;
			rrot.z += DegToRad(12.0f) * windup;

			rightArmTr_->translate_.x += 0.04f * windup;
			rightArmTr_->translate_.y += 0.02f * windup;
			rightArmTr_->translate_.z -= 0.03f * windup;
		}

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

		if (recover > 0.0f)
		{
			// 通常姿勢へ戻る補間に任せる
		}

		if (nt >= 1.0f)
		{
			meleeSwingActive_ = false;
			meleeSwingTimer_ = 0.0f;
		}
	}

	const K4E::Vector3 leftReloadRotRad =
	{
		DegToRad(reloadLeftArmRotDeg_.x * reloadT + reloadLoadLeftArmRotDeg_.x * loadSmoothT),
		DegToRad(reloadLeftArmRotDeg_.y * reloadT + reloadLoadLeftArmRotDeg_.y * loadSmoothT),
		DegToRad(reloadLeftArmRotDeg_.z * reloadT + reloadLoadLeftArmRotDeg_.z * loadSmoothT),
	};

	const K4E::Quaternion baseLeftQuat = MakeQuaternionFromEulerRad(lrot);
	const K4E::Quaternion reloadLeftQuat = MakeQuaternionFromEulerRad(leftReloadRotRad);

	leftArmTr_->useQuaternionRotation_ = true;
	leftArmTr_->rotate_ = lrot;
	leftArmTr_->quaternion_ = K4E::Quaternion::Normalize(
		K4E::Quaternion::Multiply(baseLeftQuat, reloadLeftQuat));

	const K4E::Vector3 rightReloadRotRad =
	{
		DegToRad(reloadRightArmRotDeg_.x * reloadT + reloadLoadRightArmRotDeg_.x * loadSmoothT),
		DegToRad(reloadRightArmRotDeg_.y * reloadT + reloadLoadRightArmRotDeg_.y * loadSmoothT),
		DegToRad(reloadRightArmRotDeg_.z * reloadT + reloadLoadRightArmRotDeg_.z * loadSmoothT),
	};

	const K4E::Quaternion baseRightQuat = MakeQuaternionFromEulerRad(rrot);
	const K4E::Quaternion reloadRightQuat = MakeQuaternionFromEulerRad(rightReloadRotRad);

	rightArmTr_->useQuaternionRotation_ = true;
	rightArmTr_->rotate_ = rrot;
	rightArmTr_->quaternion_ = K4E::Quaternion::Normalize(
		K4E::Quaternion::Multiply(baseRightQuat, reloadRightQuat));
}

K4E::Quaternion PlayerViewComponent::MakeQuaternionFromEulerDeg(const K4E::Vector3& eulerDeg) const
{
	return MakeQuaternionFromEulerRad({
		DegToRad(eulerDeg.x),
		DegToRad(eulerDeg.y),
		DegToRad(eulerDeg.z),
		});
}
