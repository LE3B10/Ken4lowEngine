#include "Player.h"
#include "DirectXCommon.h"
#include <Input.h>
#include <Hammer.h>
#include "Enemy.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"


/// ---------- コンボ定数表 ---------- ///
const std::array<Player::ConstAttack, Player::ComboNum> Player::kComboAttacks_ =
{ {
	{12, 10, 20, 20, 0.8f, 0.5f, 1.5f},  // 1段目: 横振り
	{12, 10, 20, 20, 0.8f, 0.5f, 1.5f},  // 2段目: 縦振り
	{12, 10, 20, 20, 0.8f, 0.5f, 1.5f}  // 3段目: 回転攻撃
} };


Player::Player()
{
	// シリアルナンバーを振る
	serialNumber_ = nextSerialNumber_;
	// 次のシリアルナンバーに1を足す
	++nextSerialNumber_;
}

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	// 基底クラスの初期化
	BaseCharacter::Initialize();
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	TextureManager::GetInstance()->LoadTexture("Resources/particle.png");

	particleManager_ = ParticleManager::GetInstance();
	particleManager_->CreateParticleGroup("fire", "Resources/particle.png");

	particleEmitter_ = std::make_unique<ParticleEmitter>(particleManager_, "fire");

	// 体（親）の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Player/body.gltf");
	body_.transform.Initialize();
	body_.transform.translation_ = { 0, 0, 0 };

	// 子オブジェクト（頭、腕）をリストに追加
	std::vector<std::pair<std::string, Vector3>> partData =
	{
		{"Player/Head.gltf", {0, 5.0f, 0}},     // 頭
		{"Player/L_arm.gltf", {0.0f, 4.5f, 0}}, // 左腕
		{"Player/R_arm.gltf", {0.0f, 4.5f, 0}}  // 右腕
	};

	for (const auto& [modelPath, position] : partData)
	{
		BodyPart part;
		part.object = std::make_unique<Object3D>();
		part.object->Initialize(modelPath);
		part.transform.Initialize();
		part.transform.translation_ = position;
		part.object->SetTranslate(part.transform.translation_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}

	// ハンマーの親をプレイヤーの体に設定
	hammer_->SetParentTransform(&body_.transform);

	// 浮遊ギミックの初期化
	InitializeFloatingGimmick();
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	// 基底クラスの更新
	BaseCharacter::Update();

	// 無敵時間のカウントダウン
	if (isInvincible_)
	{
		invincibleTimer_ -= 1.0f / dxCommon_->GetFPSCounter().GetFPS(); // 60FPS基準で計算
		if (invincibleTimer_ <= 0.0f)
		{
			isInvincible_ = false; // 無敵終了
		}
	}


	// 浮遊ギミックの更新
	if (!isJumping_ && !isAttacking_)
	{
		UpdateFloatingGimmick();
	}

	// ハンマーの更新
	if (hammer_) { hammer_->Update(); }

	// 攻撃キーを押したら攻撃開始（ゲームパッド対応）
	if ((input_->TriggerKey(DIK_F) || input_->TriggerButton(XButtons.X)) && behavior_ == Behavior::kRoot)
	{
		behaviorRequest_ = Behavior::kAttack;
	}

	// ジャンプキーを押したらジャンプ開始（ゲームパッド対応）
	if ((input_->PushKey(DIK_SPACE) || input_->TriggerButton(XButtons.A)) && behavior_ == Behavior::kRoot)
	{
		behaviorRequest_ = Behavior::kJump;
	}
	
	// ダッシュキーを押したらダッシュ開始（ゲームパッド対応）
	if ((input_->PushKey(DIK_LSHIFT) || input_->PushButton(XButtons.L_Trigger)) && behavior_ == Behavior::kRoot)
	{
		behaviorRequest_ = Behavior::kDash;
	}

	// ビヘイビア遷移
	if (behaviorRequest_)
	{
		// ビヘイビアを変更
		behavior_ = behaviorRequest_.value();

		// 各ビヘイビアごとの初期化を実行
		switch (behavior_)
		{
			/// ----- ルートビヘイビア ----- ///
		case Behavior::kRoot:
		default:

			// 通常行動の初期化
			BehaviorRootInitialize();
			break;

			/// ----- アタックビヘイビア ----- ///
		case Behavior::kAttack:

			// 攻撃行動の初期化
			BehaviorAttackInitialize();
			break;

			/// ---------- ダッシュビヘイビア ----------///
		case Behavior::kDash:

			// ダッシュ行動の初期化
			BehaviorDashInitialize();
			break;

			/// ---------- ジャンプビヘイビア ----------///
		case Behavior::kJump:

			// ジャンプ行動の初期化
			BehaviorJumpInitialize();
			break;
		}

		// ビヘイビアリクエストをリセット
		behaviorRequest_ = std::nullopt;
	}

	// ビヘイビアの実行
	switch (behavior_)
	{
		/// ----- ルートビヘイビア ----- ///
	case Behavior::kRoot:
	default:

		// 通常行動の更新
		BehaviorRootUpdate();
		break;

		/// ----- アタックビヘイビア ----- ///
	case Behavior::kAttack:

		// 攻撃行動の更新
		BehaviorAttackUpdate();
		break;

		/// ---------- ダッシュビヘイビア ----------///
	case Behavior::kDash:

		// ダッシュ行動の更新処理
		BehaviorDashUpdate();
		break;

		/// ---------- ジャンプビヘイビア ----------///
	case Behavior::kJump:

		// ジャンプ行動の初期化
		BehaviorJumpUpdate();
		break;
	}
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{

	// 通常描画
	if (!isInvincible_ || static_cast<int>(invincibleTimer_ * 10) % 2 == 0)
	{
		// 基底クラスの描画
		BaseCharacter::Draw();
	}

	// ハンマーの描画
	if (isAttacking_ && hammer_) { hammer_->Draw(); }
}


/// -------------------------------------------------------------
///						衝突時の判定処理
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{
	// 衝突相手の種別IDを取得
	uint32_t typeID = other->GetTypeID();

	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
	{
		float deltaTime = 1.0f / dxCommon_->GetFPSCounter().GetFPS();
		particleEmitter_->SetPosition({ body_.transform.translation_.x,body_.transform.translation_.y + 3.0f,body_.transform.translation_.z });
		particleEmitter_->SetEmissionRate(5000.0f);
		particleEmitter_->Update(deltaTime);
		SetDamage(1);
	}
}


/// -------------------------------------------------------------
///						中心座標を取得する処理
/// -------------------------------------------------------------
Vector3 Player::GetCenterPosition() const
{
	const Vector3 offset = { 0.0f,3.0f,0.0f };
	Vector3 worldPosition = body_.transform.translation_ + offset;
	return worldPosition;
}


void Player::SetDamage(int damage)
{
	if (!isInvincible_)
	{
		playerHP_ -= damage;
		if (playerHP_ <= 0)
		{
			isDead_ = true;
		}
		isInvincible_ = true;           // 無敵フラグをON
		invincibleTimer_ = invincibleDuration_; // 無敵時間をリセット
	}
}

/// -------------------------------------------------------------
///							移動処理
/// -------------------------------------------------------------
void Player::Move()
{
	if (camera_)
	{
		Vector3 move = { 0, 0, 0 };

		// 入力処理
		if (input_->PushKey(DIK_W)) { move.z += moveSpeed_; } // 前進
		if (input_->PushKey(DIK_S)) { move.z -= moveSpeed_; } // 後退
		if (input_->PushKey(DIK_A)) { move.x -= moveSpeed_; } // 左移動
		if (input_->PushKey(DIK_D)) { move.x += moveSpeed_; } // 右移動

		// 入力処理（ゲームパッドの左スティック）
		Vector2 leftStick = input_->GetLeftStick();
		if (!input_->LStickInDeadZone()) // デッドゾーンでなければ移動
		{ 
			move.x += leftStick.x * moveSpeed_;
			move.z += leftStick.y * moveSpeed_;
		}

		if (move.x != 0 || move.z != 0) // 移動している時だけ向きを変える
		{
			float yaw = camera_->GetRotate().y;

			// Yawによる移動ベクトルの回転
			Vector3 rotatedMove;
			rotatedMove.x = move.x * cosf(yaw) - move.z * sinf(yaw);
			rotatedMove.z = move.x * sinf(yaw) + move.z * cosf(yaw);

			// 向きを滑らかに補間 (度数法に変換)
			float targetAngle = atan2(-rotatedMove.x, rotatedMove.z) * (180.0f / std::numbers::pi_v<float>);
			float smoothAngle = Vector3::AngleLerp(body_.transform.rotate_.y * (180.0f / std::numbers::pi_v<float>), targetAngle, 0.2f);
			body_.transform.rotate_.y = smoothAngle * (std::numbers::pi_v<float> / 180.0f);

			// 正規化して移動
			rotatedMove = Vector3::Normalize(rotatedMove);
			body_.transform.translation_ += rotatedMove * moveSpeed_;
		}
	}
}


/// -------------------------------------------------------------
///					　浮遊ギミックの初期化処理
/// -------------------------------------------------------------
void Player::InitializeFloatingGimmick()
{
	// 浮遊ギミックの初期化
	floatingParameter_ = 0.0f;
}


/// -------------------------------------------------------------
///					　浮遊ギミックの更新処理
/// -------------------------------------------------------------
void Player::UpdateFloatingGimmick()
{
	// 浮遊ギミックの更新
	floatingParameter_ += kFloatingStep;
	// 2πを超えたら0に戻す
	floatingParameter_ = std::fmodf(floatingParameter_, 2.0f * std::numbers::pi_v<float>);
	// 浮遊の振幅<m>
	const float floatingAmplitude = 0.3f;
	// 浮遊を座標に適用
	body_.transform.translation_.y = floatingAmplitude * sinf(floatingParameter_);
}


/// -------------------------------------------------------------
///					　腕のアニメーション更新処理
/// -------------------------------------------------------------
void Player::UpdateArmAnimation(bool isMoving)
{
	// 移動しているときは腕を速く振る
	float swingSpeed = isMoving ? 0.1f : 0.052f; // 移動時と待機時で速度を変える

	// アニメーションパラメータの更新
	armSwingParameter_ += swingSpeed;

	// 2πを超えたらリセット（ループさせる）
	armSwingParameter_ = std::fmod(armSwingParameter_, 2.0f * std::numbers::pi_v<float>);

	// 目標の振り角度をサイン波で計算
	float targetSwingAngle = kMaxArmSwingAngle * sinf(armSwingParameter_);

	// 各部位のアニメーションを適用
	if (parts_.size() >= 2) // 腕のデータが存在するか確認
	{
		// 左腕（現在の角度から目標角度に補間）
		float smoothedLeft = Vector3::Lerp(parts_[1].transform.rotate_.x, isMoving ? -targetSwingAngle : -targetSwingAngle * 0.5f, 0.2f);
		parts_[1].transform.rotate_.x = smoothedLeft;

		// 右腕（現在の角度から目標角度に補間）
		float smoothedRight = Vector3::Lerp(parts_[2].transform.rotate_.x, isMoving ? targetSwingAngle : -targetSwingAngle * 0.5f, 0.2f);
		parts_[2].transform.rotate_.x = smoothedRight;
	}
}


/// -------------------------------------------------------------
///					　通常行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorRootInitialize()
{
	isAttacking_ = false; // ハンマーを非表示
	attackFrame_ = 0; // 攻撃フレームリセット
	isJumping_ = false; // ジャンプフラグをリセット
}


/// -------------------------------------------------------------
///					　通常行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorRootUpdate()
{
	// 移動処理
	Move();

	// 腕のアニメーションを更新（移動中かどうか判定）
	bool isMoving = input_->PushKey(DIK_W) || input_->PushKey(DIK_S) || input_->PushKey(DIK_A) || input_->PushKey(DIK_D) || !input_->LStickInDeadZone();
	UpdateArmAnimation(isMoving);
}


/// -------------------------------------------------------------
///					　	攻撃行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorAttackInitialize()
{
	attackFrame_ = 0; // 攻撃の進行フレームをリセット
	isAttacking_ = true; // ハンマーを表示
	workAttack_.comboNext = false; // 次のコンボ受付をリセット
	hammer_->ClearContactRecord(); // ハンマーの接触履歴を削除
}


/// -------------------------------------------------------------
///					　	攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorAttackUpdate()
{
	attackFrame_++; // 腕の回転を適用（両腕同じ動き）
	float playerYaw = body_.transform.rotate_.y; // プレイヤーのY軸回転
	float armAngle = 0.0f; // 腕の回転角度

	const ConstAttack& attackData = kComboAttacks_[workAttack_.conboIndex];

	int attackTotalFrames_ = attackData.anticipationTime + attackData.swingTime + attackData.recoveryTime;
	int attackSwingFrames_ = attackData.swingTime;
	int attackHoldFrames_ = attackData.recoveryTime;

	// 腕の角度補間
	if (workAttack_.inComboPhase == 0)
	{
		float t = static_cast<float>(attackFrame_) / attackSwingFrames_;
		armAngle = -maxArmSwingAngle_ * (1.0f + t);
	}
	else if (workAttack_.inComboPhase == 1)
	{
		float t = static_cast<float>(attackFrame_) / (attackTotalFrames_ - attackSwingFrames_);
		armAngle = -maxArmSwingAngle_ * (2.0f - t);
	}
	else
	{
		armAngle = maxArmSwingAngle_ * -1.0f;
	}

	switch (workAttack_.inComboPhase)
	{
	case 0: // 振りかぶり
		if (attackFrame_ < attackData.anticipationTime)
		{
			float t = static_cast<float>(attackFrame_) / attackData.anticipationTime;
			hammer_->SetRotation({ 1.7f * (1.0f - t), body_.transform.rotate_.y, 0.0f });
		}
		else
		{
			workAttack_.inComboPhase = 1;
			attackFrame_ = 0;
		}
		break;

	case 1: // 振り下ろし
		if (attackFrame_ < attackData.swingTime)
		{
			float t = static_cast<float>(attackFrame_) / attackData.swingTime;
			hammer_->SetRotation({ 1.7f * t, body_.transform.rotate_.y, 0.0f });
		}
		else
		{
			workAttack_.inComboPhase = 2;
			attackFrame_ = 0;
		}
		break;

	case 2: // 硬直 & コンボ受付
		if (input_->TriggerKey(DIK_F) || input_->TriggerButton(XButtons.X))
		{
			workAttack_.comboNext = true;
		}

		if (attackFrame_ >= attackData.recoveryTime)
		{
			if (workAttack_.comboNext && workAttack_.conboIndex + 1 < ComboNum)
			{
				workAttack_.conboIndex++;
				workAttack_.inComboPhase = 0;
				BehaviorAttackInitialize();
			}
			else
			{
				behaviorRequest_ = Behavior::kRoot;
				isAttacking_ = false;
				workAttack_.conboIndex = 0;
				workAttack_.inComboPhase = 0;
			}
		}
		break;
	}

	// 腕の回転を適用（両腕同じ動き）
	if (parts_.size() >= 2) // 腕のデータが存在するか確認
	{
		parts_[1].transform.rotate_.x = armAngle; // 左腕
		parts_[2].transform.rotate_.x = armAngle; // 右腕
	}
}


/// -------------------------------------------------------------
///					　ダッシュ行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorDashInitialize()
{
	workDash_.dashParameter_ = 0; // カウンターリセット

	float yaw = body_.transform.rotate_.y; // プレイヤーのY軸回転を使用
	dashDirection_.x = -sinf(yaw);
	dashDirection_.z = cosf(yaw);

	dashDirection_ = Vector3::Normalize(dashDirection_); // 正規化
}



/// -------------------------------------------------------------
///					　	ダッシュ行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorDashUpdate()
{
	// ダッシュの時間（frame）
	const uint32_t behaviorDashTime = 20;

	// ダッシュの移動
	body_.transform.translation_ += dashDirection_ * dashSpeed_;

	// 向きを固定
	body_.transform.rotate_.y = atan2(-dashDirection_.x, dashDirection_.z);

	// ダッシュ時間が終了したら通常行動に戻る
	if (++workDash_.dashParameter_ >= behaviorDashTime)
	{
		behaviorRequest_ = Behavior::kRoot;
	}
}


/// -------------------------------------------------------------
///					　	ジャンプ行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorJumpInitialize()
{
	if (!isJumping_)
	{
		isJumping_ = true;
		jumpVelocity_ = jumpPower_; // ジャンプ初速度を設定
	}
}


/// -------------------------------------------------------------
///					　	ジャンプ行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorJumpUpdate()
{
	if (isJumping_)
	{
		// 重力を適用
		jumpVelocity_ -= gravity_;

		// 位置を更新
		body_.transform.translation_.y += jumpVelocity_;

		// 移動処理
		Move();

		// 地面に着いたらジャンプ終了
		if (body_.transform.translation_.y <= 0.0f)
		{
			body_.transform.translation_.y = 0.0f;
			isJumping_ = false;
			behaviorRequest_ = Behavior::kRoot; // 通常状態に戻す
		}
	}
}
