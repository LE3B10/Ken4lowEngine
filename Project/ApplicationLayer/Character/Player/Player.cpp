#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include "LinearInterpolation.h"

#include <imgui.h>
#include <algorithm>


/// -------------------------------------------------------------
///				　			　 デストラクタ
/// -------------------------------------------------------------
Player::~Player()
{

}

/// -------------------------------------------------------------
///				　			　 初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	input_ = Input::GetInstance();

	// ID登録
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("PlayerRoot/player_body.gltf");

	// 初期位置
	body_.transform.translate_ = { 0.0f, 2.25f, 0.0f };

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"PlayerRoot/player_head.gltf", {0.0f, 0.75f, 0.0f}},     // 頭
		{"PlayerRoot/player_LeftArm.gltf", {-0.75f, 0.75f, 0.0f}}, // 左腕
		{"PlayerRoot/player_RightArm.gltf",{ 0.75f, -0.75f, 0}},  // 右腕
		{"PlayerRoot/player_LeftLeg.gltf", {-0.25f, -0.75f, 0} },  // 左脚
		{"PlayerRoot/player_RightLeg.gltf", {0.25f, -0.75f, 0} }  // 右脚
	};

	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}

	// FPSカメラ
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(this);

	groundY_ = body_.transform.translate_.y;  // 今立っている高さを床として扱う
	isGrounded_ = true;
	vY_ = 0.0f;
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{

	if (input_->TriggerKey(DIK_F5)) { fpsCamera_->CycleViewMode(); }

	// ここで表示切替
	switch (fpsCamera_->GetViewMode()) // ← カメラの現在モード
	{
	case FpsCamera::ViewMode::FirstPerson:
		SetBodyActive(false);          // 体幹は映さない
		SetAllPartsActive(false);      // 一旦全部オフ
		SetPartActive(rightArmIndex_, true); // 右手だけオン
		break;

	case FpsCamera::ViewMode::ThirdBack:
	case FpsCamera::ViewMode::ThirdFront:
		SetBodyActive(true);
		SetAllPartsActive(true);       // 全パーツオン
		break;
	}

	Move();

	BaseCharacter::Update();
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	if (body_.active) { body_.object->Draw(); }

	for (auto& part : parts_) {
		if (part.active) { part.object->Draw(); }
	}
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{

}

/// -------------------------------------------------------------
///				　		  中心座標を取得する純粋仮想関数
/// -------------------------------------------------------------
Vector3 Player::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,3.0f,0.0f };
	Vector3 worldPosition = body_.transform.translate_ + offset;
	return worldPosition;
}

/// -------------------------------------------------------------
///				　			　 移動処理
/// -------------------------------------------------------------
void Player::Move()
{
	fpsCamera_->Update();

	// 仮実装
	const float moveSpeed = 0.1f;
	Vector3 move{ 0,0,0 };

	if (!isDebugCamera_) // デバッグカメラ中は無効
	{
		if (input_->PushKey(DIK_W)) move.z += moveSpeed;
		if (input_->PushKey(DIK_S)) move.z -= moveSpeed;
		if (input_->PushKey(DIK_A)) move.x -= moveSpeed;
		if (input_->PushKey(DIK_D)) move.x += moveSpeed;
	}

	// 斜め移動補正
	if (Vector3::Length(move) > 0.0f) move = Vector3::Normalize(move) * moveSpeed;

	if (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::ThirdFront)
	{
		move.z *= -1.0f;
	}

	const float yaw = fpsCamera_->GetCamera()->GetRotate().y;
	const float s = std::sinf(yaw), c = std::cosf(yaw);
	move = { move.x * c - move.z * s, 0.0f, move.x * s + move.z * c };

	// 体の移動
	body_.transform.translate_ += move;

	const bool  isFP = (fpsCamera_->GetViewMode() == FpsCamera::ViewMode::FirstPerson);
	const float camYaw = fpsCamera_->GetYaw();
	const float camPitch = fpsCamera_->GetPitch();

	if (isFP)
	{
		// 一人称：回転は即時
		bodyYaw_ = camYaw;
		headYawLocal_ = 0.0f;
		parts_[0].transform.rotate_.x = std::clamp(camPitch, -headPitchLimit_, headPitchLimit_);
	}
	else
	{
		// 三人称：補間（現状のまま）
		float targetHeadYawLocal = NormalizeAngle(camYaw - bodyYaw_);
		targetHeadYawLocal = std::clamp(targetHeadYawLocal, -headYawLimit_, headYawLimit_);
		headYawLocal_ = Lerp(headYawLocal_, targetHeadYawLocal, 0.25f);
		bodyYaw_ = LerpAngle(bodyYaw_, camYaw, 0.10f);
		parts_[0].transform.rotate_.x = std::clamp(camPitch, -headPitchLimit_, headPitchLimit_);
	}

	// ジャンプ
	if (!isDebugCamera_ && isGrounded_ && input_->PushKey(DIK_SPACE))
	{
		vY_ = jumpSpeed_;
		isGrounded_ = false;
	}

	const float dt = 1.0f / 60.0f;          // 必要なら実Δtに置換

	// 空中は重力適用
	if (!isGrounded_)
	{
		vY_ += gravity_ * dt;
		if (vY_ < maxFallSpeed_) vY_ = maxFallSpeed_;
		body_.transform.translate_.y += vY_ * dt;

		// 着地（水平床の想定）
		if (body_.transform.translate_.y <= groundY_)
		{
			body_.transform.translate_.y = groundY_;
			vY_ = 0.0f;
			isGrounded_ = true;
		}
	}
	else
	{
		// 接地中は高さを維持（段差に対応するなら groundY_ を更新する）
		body_.transform.translate_.y = groundY_;
	}

	body_.transform.rotate_.y = bodyYaw_;
	parts_[0].transform.rotate_.y = headYawLocal_;

	// ===== 右手：FPはカメラ基準で“固定” =====
	auto& armT = parts_[rightArmIndex_].transform;
	if (isFP)
	{
		armT.parent_ = nullptr;  // ★親を外してワールド直置きにする

		// 右下・前に出すカメラローカルオフセット（好みで微調整）
		Vector3 offset = { 0.75f, -0.75f, 0.75 };

		// カメラ姿勢でオフセットを回す（Yaw→Pitch）
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(camYaw);
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(camPitch);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);
		offset = Matrix4x4::TransformCoord(offset, R);

		// カメラ位置＋回したオフセット（＝画面に“固定”される）
		const Vector3 camPos = fpsCamera_->GetCamera()->GetTranslate();
		armT.translate_ = camPos + offset;
		armT.rotate_.y = camYaw;
		armT.rotate_.x = camPitch - (90.0f * std::numbers::pi_v<float> / 180.0f);
	}
	else
	{
		// 三人称では元の階層に戻す
		armT.parent_ = &body_.transform;
		armT.translate_ = { 0.75f, 0.75f, 0 };
		armT.rotate_ = { 0.0f, 0.0f, 0.0f };     // ← yaw も roll もゼロに
		fpsCamera_->Update();
	}
}