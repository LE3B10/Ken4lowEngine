#include "Player.h"
#include <Input.h>


/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize()
{
	// 基底クラスの初期化
	BaseCharacter::Initialize();

	// 体（親）の初期化
	body_.object = std::make_unique<Object3D>();
	body_.object->Initialize("Player/body.gltf");
	body_.transform.Initialize();
	body_.transform.translate_ = { 0, 0, 0 };

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
		part.transform.translate_ = position;
		part.object->SetTranslate(part.transform.translate_);
		part.transform.parent_ = &body_.transform; // 親を設定
		parts_.push_back(std::move(part));
	}

	// ハンマーの初期化
	hammer_ = std::make_unique<Hammer>();
	hammer_->Initialize();

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

	// 浮遊ギミックの更新
	UpdateFloatingGimmick();

	// ハンマーの更新
	if (hammer_) { hammer_->Update(); }

	// 攻撃キー（例えばスペースキー）を押したら攻撃を開始
	if (input_->PushKey(DIK_SPACE) && behavior_ == Behavior::kRoot) { behaviorRequest_ = Behavior::kAttack; }

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
	}
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 基底クラスの描画
	BaseCharacter::Draw();

	// ハンマーの描画
	if (isAttacking_ && hammer_) { hammer_->Draw(); }
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
			body_.transform.translate_ += rotatedMove * moveSpeed_;
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
	body_.transform.translate_.y = floatingAmplitude * sinf(floatingParameter_);
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
	isAttackHold_ = false; // 固定フラグ解除
	attackFrame_ = 0; // 攻撃フレームリセット
	attackHoldFrame_ = 0; // 硬直フレームリセット

	// 腕の角度をリセット
	if (parts_.size() >= 2)
	{
		parts_[1].transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // 左腕リセット
		parts_[2].transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // 右腕リセット
	}
}


/// -------------------------------------------------------------
///					　通常行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorRootUpdate()
{
	// 移動処理
	Move();

	// 腕のアニメーションを更新（移動中かどうか判定）
	bool isMoving = input_->PushKey(DIK_W) || input_->PushKey(DIK_S) || input_->PushKey(DIK_A) || input_->PushKey(DIK_D);
	UpdateArmAnimation(isMoving);
}


/// -------------------------------------------------------------
///					　	攻撃行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorAttackInitialize()
{
	attackFrame_ = 0; // 攻撃の進行フレームをリセット
	isAttacking_ = true; // ハンマーを表示
	isAttackHold_ = false; // 固定フラグをリセット
	attackHoldFrame_ = 0; // 硬直フレームリセット
}


/// -------------------------------------------------------------
///					　	攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorAttackUpdate()
{
	attackFrame_++; // 腕の回転を適用（両腕同じ動き）

	float playerYaw = body_.transform.rotate_.y; // プレイヤーのY軸回転
	float armAngle = 0.0f; // 腕の回転角度

	if (attackFrame_ < attackSwingFrames_)
	{
		// 振りかぶり（前半）
		float t = static_cast<float>(attackFrame_) / attackSwingFrames_;
		float angle = -1.7f * (1.0f - t); // -90度 → 0度
		hammer_->SetRotation({ -angle, playerYaw, 0.0f });

		armAngle = -maxArmSwingAngle_ * (1.0f + t); // 腕を後ろに引く
	}
	else if (attackFrame_ < attackTotalFrames_)
	{
		// 振り下ろし（後半）
		float t = static_cast<float>(attackFrame_ - attackSwingFrames_) / (attackTotalFrames_ - attackSwingFrames_);
		float angle = -1.7f * t; // 0度 → -90度
		hammer_->SetRotation({ -angle, playerYaw, 0.0f });

		armAngle = -maxArmSwingAngle_ * (2.0f - t); // 腕を前に振る
	}
	else
	{
		// 攻撃アニメーション終了 → 角度を固定
		if (!isAttackHold_)
		{
			isAttackHold_ = true; // 固定モードに切り替え
			attackHoldFrame_ = 0; // カウンターリセット
		}

		// 攻撃後の硬直中の腕の角度（少し下げた状態）
		armAngle = maxArmSwingAngle_ * -1.0f;

		// 固定時間が経過するまでハンマーの角度を変更しない
		attackHoldFrame_++;
		if (attackHoldFrame_ >= attackHoldFrames_)
		{
			// 硬直が終わったら通常状態に戻る
			behaviorRequest_ = Behavior::kRoot;
			isAttacking_ = false; // ハンマーを非表示
			isAttackHold_ = false; // フラグリセット
		}
	}

	// 腕の回転を適用（両腕同じ動き）
	if (parts_.size() >= 2) // 腕のデータが存在するか確認
	{
		parts_[1].transform.rotate_.x = armAngle; // 左腕
		parts_[2].transform.rotate_.x = armAngle; // 右腕
	}
}
