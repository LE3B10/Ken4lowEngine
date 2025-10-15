#define NOMINMAX
#include "Player.h"
#include <Object3DCommon.h>
#include <CollisionTypeIdDef.h>
#include <Input.h>
#include <AudioManager.h>
#include <PostEffectManager.h>

#include <imgui.h>


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

	// 各部位の初期化
	InitializeBodyParts();

	// プレイヤーコントローラー
	playerController_ = std::make_unique<PlayerController>();
	playerController_->Initialize(parts_[kBody].object.get());

	// FPSカメラ
	fpsCamera_ = std::make_unique<FpsCamera>();
	fpsCamera_->Initialize(this);
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	if (isDebugCamera_) return; // デバッグカメラ中は無効

	float dt = 1.0f / 60.0f;
	float speed = 3.0f;

	// カメラを更新
	fpsCamera_->SetDeltaTime(dt);
	fpsCamera_->Update();
	float yaw = fpsCamera_->GetYaw();
	float pitch = fpsCamera_->GetPitch();

	// 体と頭へ反映
	parts_[kBody].rotate.y = yaw;
	parts_[kHead].rotate.x = pitch;

	// --- FPS視点では頭を非表示 ---
	bool firstPerson = true;  // 切替したければフラグ化
	parts_[kBody].visible = !firstPerson;
	parts_[kHead].visible = !firstPerson;

	// 移動入力
	playerController_->UpdateMovement(fpsCamera_->GetCamera(), dt, false);

	// 移動反映
	worldPosition_ = parts_[kBody].object->GetTranslate();

	// 親子伝播（既存式そのまま）
	UpdateHierarchy();

	// 見た目にプレイヤー位置を加算して反映（既存APIのみ）
	for (auto& p : parts_)
	{
		if (!p.object) continue;

		// 位置
		Vector3 worldT{ p.worldMatrix.m[3][0], p.worldMatrix.m[3][1], p.worldMatrix.m[3][2] };
		p.object->SetTranslate(worldT + worldPosition_);
		p.object->Update();
	}
}


/// -------------------------------------------------------------
///				　			　 描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	for (auto& p : parts_)
	{
		if (p.visible && p.object) p.object->Draw();
	}
}


/// -------------------------------------------------------------
///				　		  ImGui描画処理
/// -------------------------------------------------------------
void Player::DrawImGui()
{

}

/// -------------------------------------------------------------
///				　		 各部位の初期化
/// -------------------------------------------------------------
void Player::InitializeBodyParts()
{
	// プレイヤーモデルの生成
	parts_.resize(6); // 部位数分リサイズ

	auto LoadPart = [&](int index, const std::string& modelPath, const std::string& name, int parent) {
		parts_[index].name = name;									 // 部位名
		parts_[index].parentIndex = parent;							 // 親のインデックス
		parts_[index].scale = { 1.0f, 1.0f, 1.0f };					 // 初期スケール
		parts_[index].rotate = { 0.0f, 0.0f, 0.0f };				 // 初期回転
		parts_[index].traslate = { 0.0f, 0.0f, 0.0f };				 // 初期位置
		parts_[index].object = std::make_unique<Object3D>();		 // Object3Dの生成
		parts_[index].object->Initialize("PlayerRoot/" + modelPath); // "PlayerRoot/"を付加してパスを指定
		};

	LoadPart(kBody, "player_body.gltf", "Body", -1);				// 体（親なし）
	LoadPart(kHead, "player_head.gltf", "Head", kBody);				// 頭
	LoadPart(kLeftArm, "player_LeftArm.gltf", "LeftArm", kBody);	// 左腕
	LoadPart(kRightArm, "player_RightArm.gltf", "RightArm", kBody); // 右腕
	LoadPart(kLeftLeg, "player_LeftLeg.gltf", "LeftLeg", kBody);	// 左脚
	LoadPart(kRightLeg, "player_RightLeg.gltf", "RightLeg", kBody); // 右脚

	parts_[kBody].traslate = { 0.0f, 0.0f, 0.0f };   // 親なのでそのまま
	parts_[kHead].traslate = { 0.0f, 1.5f, 0.0f };  // 腰から首まで+0.4m
	parts_[kLeftArm].traslate = { -1.5f, 1.5f, 0.0f }; // 左肩へ（Xマイナス, Zプラス）
	parts_[kRightArm].traslate = { +1.5f, 1.5f, 0.0f }; // 右肩へ
	parts_[kLeftLeg].traslate = { 0.5f, -1.5f, 0.00f }; // 股関節は腰と同Z、Xだけ外側へ
	parts_[kRightLeg].traslate = { -0.5f, -1.5f, 0.0f }; // 右脚も同様
}
