#include "Player.h"
#include "Object3DCommon.h"
#include "VectorMath.h"
#include "MatrixMath.h"
#include "ParameterManager.h"

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon, Camera* camera)
{
	BaseCharacter::Initialize(object3DCommon, camera);

	ParameterManager* parameterManagers = ParameterManager::GetInstance();
	const char* groupName = "Player";

	// グループを追加
	ParameterManager::GetInstance()->CreateGroup(groupName);
	parameterManagers->AddItem(groupName, "Test", 90);

	// 浮遊ギミックの初期化
	InitializeFlaotingGimmick();

	parts_ = {
		// 胴体（親なし）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}, nullptr, "Player/body.gltf", -1 },

		// 左腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {-0.5f, 4.5f, 0.0f}}, nullptr, "Player/L_Arm.gltf", 0 },

		// 右腕（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 4.5f, 0.0f}}, nullptr, "Player/R_Arm.gltf", 0 },

		// 頭（親は胴体）
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Player/Head.gltf", 0 },

		// 武器
		{ {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f}}, nullptr, "Hammer/Hammer.gltf", 0 }
	};

	for (size_t i = 0; i < parts_.size(); ++i)
	{
		auto& part = parts_[i];
		part.object3D = std::make_unique<Object3D>();
		part.object3D->Initialize(object3DCommon, part.modelFile);
		part.object3D->SetTranslate(part.worldTransform.translate);

		// 子オブジェクトのローカル座標を記録
		if (part.parentIndex != -1)
		{
			part.localOffset = part.worldTransform.translate - parts_[part.parentIndex].worldTransform.translate;
		}
	}

	// プレイヤーの初期回転をカメラの角度に合わせる
	if (camera_)
	{
		float cameraYaw = camera_->GetYaw();
		parts_[0].worldTransform.rotate.y = cameraYaw; // カメラ基準で補正
	}
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	BaseCharacter::Update();

	// **攻撃中は浮遊ギミックを更新しない**
	if (behavior_ != Behavior::kAttack && behavior_ != Behavior::kJump)
	{
		// 浮遊ギミックの更新処理
		UpdateFloatingGimmick();
	}

	if (behaviorRequest_)
	{
		behavior_ = behaviorRequest_.value();

		switch (behavior_)
		{
		case Behavior::kRoot:
			// 通常行動の初期化処理
			BehaviorRootInitialize();
			break;
		case Behavior::kAttack:
			// 攻撃行動の初期化処理
			BehaviorAttackInitialize();
			break;
		case Behavior::kDash:
			// ダッシュ行動の初期化処理
			BehaviorDashInitialize();
			break;
		case Behavior::kJump:
			// ジャンプ行動の初期化処理
			BehaviorJumpInitialize();
			break;
		}

		behaviorRequest_ = std::nullopt;
	}

	switch (behavior_)
	{
	case Behavior::kRoot:
		// 通常行動の更新処理
		BehaviorRootUpdate();
		break;
	case Behavior::kAttack:
		// 攻撃行動の更新処理
		BehaviorAttackUpdate();
		break;
	case Behavior::kDash:
		// ダッシュ行動の更新処理
		BehaviorDashUpdate();
		break;
	case Behavior::kJump:
		// ジャンプ行動の更新処理
		BehaviorJumpUpdate();
		break;
	}

	// **親子関係の計算より前に適用**
	for (size_t i = 1; i < parts_.size(); ++i)
	{
		int parentIndex = parts_[i].parentIndex;
		if (parentIndex != -1)
		{
			const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
			Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
			Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

			parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
			parts_[i].worldTransform.rotate.y = parentRotation.y; // Y回転のみ親と同じにする
		}
	}

	// **カメラのターゲットを更新**
	if (camera_)
	{
		camera_->SetTargetPosition(parts_[0].worldTransform.translate);
		camera_->Update();
	}
}

/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	for (size_t i = 0; i < parts_.size(); ++i)
	{
		// **武器の描画ON/OFFを切り替え**
		if (i == 4 && !isWeaponVisible_)
		{
			continue; // 武器が非表示なら描画しない
		}

		parts_[i].object3D->Draw();
	}
}


/// -------------------------------------------------------------
///					調整項目の適用処理
/// -------------------------------------------------------------
void Player::ApplyGlobalVariables()
{

}


/// -------------------------------------------------------------
///					浮遊ギミックの初期化処理
/// -------------------------------------------------------------
void Player::InitializeFlaotingGimmick()
{
	floatingParameter_ = 0.0f;
}


/// -------------------------------------------------------------
///					 浮遊ギミックの更新処理
/// -------------------------------------------------------------
void Player::UpdateFloatingGimmick()
{
	// 浮遊の振幅<m>
	const float amplitude = 0.4f; // 浮遊の振幅
	const float floatingSpeed = 0.02f; // 浮遊の速度を遅くする

	// パラメータを1ステップ分加算
	floatingParameter_ += step;
	// ２πを超えたら0戻す
	floatingParameter_ = std::fmod(floatingParameter_, 2.0f * PI);

	parts_[0].worldTransform.translate.y = std::sin(floatingParameter_) * amplitude;
}


/// -------------------------------------------------------------
///					 通常行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorRootInitialize()
{
	// **攻撃後に武器を非表示**
	isWeaponVisible_ = false;

	// 攻撃のリセット
	attackPhase_ = AttackPhase::kRaise;
	attackTimer_ = 0.0f;

	// 右腕の回転を元の角度に戻す
	parts_[2].worldTransform.rotate.x = 0.0f;
	parts_[1].worldTransform.rotate.x = 0.0f;

	// 武器の位置・回転をリセット
	parts_[4].worldTransform.rotate = { 0.0f, 0.0f, 0.0f };
	parts_[4].worldTransform.translate = parts_[2].worldTransform.translate; // 右腕と同期

	// 移動速度をリセット
	velocity = { 0.0f, 0.0f, 0.0f };

	// 各部位のオブジェクトの位置と回転を更新
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// プレイヤーの位置を基準にカメラ位置を調整
	if (camera_)
	{
		camera_->SetTargetPosition(parts_[0].worldTransform.translate);
		camera_->Update();
	}
}


/// -------------------------------------------------------------
///					 通常行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorRootUpdate()
{
	// 移動量を初期化
	Vector3 movement = { 0.0f, 0.0f, 0.0f };

	// **キー入力で攻撃に遷移**
	if (input_->TriggerKey(DIK_F)) {  // Fキーで攻撃
		behaviorRequest_ = Behavior::kAttack;
		return;  // すぐに処理を抜けて攻撃へ移行
	}

	// ダッシュボタンを押したら
	if (input_->PushKey(DIK_LSHIFT))
	{
		behaviorRequest_ = Behavior::kDash;
		return;
	}

	if (input_->TriggerKey(DIK_SPACE) && behavior_ == Behavior::kRoot)
	{
		behaviorRequest_ = Behavior::kJump; // ジャンプ開始
		return;
	}

	// 入力取得 (移動)
	if (input_->PushKey(DIK_W)) { movement.z += 1.0f; }
	if (input_->PushKey(DIK_S)) { movement.z -= 1.0f; }
	if (input_->PushKey(DIK_A)) { movement.x -= 1.0f; }
	if (input_->PushKey(DIK_D)) { movement.x += 1.0f; }

	float targetYaw = parts_[0].worldTransform.rotate.y; // 現在の角度を基準に

	// カメラの向きに基づいて移動方向を計算
	if (camera_)
	{
		Vector3 forward = camera_->GetForwardDirection();
		forward.y = 0.0f; // 水平移動のためY成分をゼロにする

		if (Length(forward) > 0.001f) {
			forward = Normalize(forward);
		}
		else {
			forward = Vector3(0.0f, 0.0f, 1.0f); // デフォルト方向を設定
		}

		Vector3 right = Normalize(Cross(Vector3(0.0f, 1.0f, 0.0f), forward)); // 水平方向の右ベクトル

		Vector3 adjustedMovement = -forward * movement.z + -right * movement.x;
		adjustedMovement.y = 0.0f;

		// 胴体（親）の移動を適用
		parts_[0].worldTransform.translate += adjustedMovement * 0.3f; // 移動速度を調整

		// 移動している場合のみ目標角度を計算
		if (Length(adjustedMovement) > 0.001f)
		{
			Vector3 forwardDirection = Normalize(adjustedMovement);
			targetYaw = atan2f(-forwardDirection.x, forwardDirection.z); // 移動方向を向く
		}
	}

	// 現在の角度と目標角度を補間
	float currentYaw = parts_[0].worldTransform.rotate.y;
	parts_[0].worldTransform.rotate.y = LerpShortAngle(currentYaw, targetYaw, 0.1f); // t=0.1で補間

	// 腕の揺れのパラメータを常に更新
	armSwingParameter_ += step * armSwingSpeed_;

	// 揺れの大きさを移動量に応じて変化させる（移動時は大きく、静止時は小さめ）
	float swingAmplitude = (Length(movement) > 0.001f) ? armSwingAmplitude_ : armSwingAmplitude_ * 0.5f;

	// 移動しているかどうかで揺れ方を変える
	float leftArmSwing, rightArmSwing;
	if (Length(movement) > 0.001f) {
		// 移動時：交互に揺れる
		leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
		rightArmSwing = std::sin(armSwingParameter_ + PI) * swingAmplitude;
	}
	else {
		// 静止時：同じ方向に揺れる
		leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
		rightArmSwing = leftArmSwing; // 左右同じ揺れ
	}

	// 両腕の回転に適用
	parts_[1].worldTransform.rotate.x = leftArmSwing;  // 左腕
	parts_[2].worldTransform.rotate.x = rightArmSwing; // 右腕

	// 各部位の最終的な位置と回転を計算
	for (size_t i = 1; i < parts_.size(); ++i)
	{
		int parentIndex = parts_[i].parentIndex;
		if (parentIndex != -1)
		{
			const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
			Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
			Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

			parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
			parts_[i].worldTransform.rotate.y = parentRotation.y; // Y回転のみ親と同じにする
		}
	}

	// 各部位のオブジェクトの位置と回転を更新
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);  // 回転を適用
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// プレイヤーの位置
	const Vector3& playerPosition = parts_[0].worldTransform.translate;

	// カメラ位置を線形補間でスムーズに移動
	if (camera_)
	{
		camera_->SetTargetPosition(playerPosition);
		camera_->Update();
	}
}


/// -------------------------------------------------------------
///					 攻撃行動の初期化処理
/// -------------------------------------------------------------
void Player::BehaviorAttackInitialize()
{
	attackPhase_ = AttackPhase::kRaise;
	attackTimer_ = 0.0f;

	// **武器を攻撃時に表示**
	isWeaponVisible_ = true;

	// **両腕を振り上げる**
	parts_[2].worldTransform.rotate.x = -PI / 2.0f;  // 右腕
	parts_[1].worldTransform.rotate.x = -PI / 2.0f;  // 左腕

	// **武器の回転リセット**
	parts_[4].worldTransform.rotate.x = PI / 2.0f;
}


/// -------------------------------------------------------------
///					 攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorAttackUpdate()
{
	const float raiseDuration = 45.0f; // 振り上げ時間
	const float swingDuration = 10.0f;  // 振り下ろし時間

	attackTimer_++;

	switch (attackPhase_)
	{
	case AttackPhase::kRaise:
		// **両腕を振り上げる**
		parts_[2].worldTransform.rotate.x -= 0.05f;  // 右腕
		parts_[1].worldTransform.rotate.x -= 0.05f;  // 左腕
		parts_[4].worldTransform.rotate.x -= 0.05f;  // 武器

		if (attackTimer_ >= raiseDuration)
		{
			attackPhase_ = AttackPhase::kSwing;
			attackTimer_ = 0.0f;
		}
		break;

	case AttackPhase::kSwing:
		// **両腕を振り下ろす**
		parts_[2].worldTransform.rotate.x += 0.25f;  // 右腕
		parts_[1].worldTransform.rotate.x += 0.25f;  // 左腕
		parts_[4].worldTransform.rotate.x += 0.25f;  // 武器

		if (attackTimer_ >= swingDuration)
		{
			attackPhase_ = AttackPhase::kEnd;
		}
		break;

	case AttackPhase::kEnd:
		// **攻撃終了後、武器を非表示にする**
		isWeaponVisible_ = false;

		// **通常行動へ戻る**
		behaviorRequest_ = Behavior::kRoot;
		break;
	}

	// **武器を右腕の位置に同期**
	//parts_[4].worldTransform.translate = parts_[2].worldTransform.translate;

	// **位置・回転の更新**
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}
}


/// -------------------------------------------------------------
///					 攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorDashInitialize()
{
	workDash_.dashParameter_ = 0;

	// 右腕の回転を元の角度に戻す
	parts_[2].worldTransform.rotate.x = PI / 2.0f;
	parts_[1].worldTransform.rotate.x = PI / 2.0f;

	// 武器の位置・回転をリセット
	parts_[4].worldTransform.rotate = { 0.0f, 0.0f, 0.0f };
	parts_[4].worldTransform.translate = parts_[2].worldTransform.translate; // 右腕と同期

	// 移動速度をリセット
	velocity = { 0.0f, 0.0f, 0.0f };

	// 各部位のオブジェクトの位置と回転を更新
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// プレイヤーの位置を基準にカメラ位置を調整
	if (camera_)
	{
		camera_->SetTargetPosition(parts_[0].worldTransform.translate);
		camera_->Update();
	}
}


/// -------------------------------------------------------------
///					 攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorDashUpdate()
{
	// **ダッシュ速度の設定**
	const float dashSpeed = 0.5f;  // 移動速度を少し抑える

	// **キー入力で攻撃に遷移**
	if (input_->TriggerKey(DIK_F)) {  // スペースキーで攻撃
		behaviorRequest_ = Behavior::kAttack;
		return;  // すぐに処理を抜けて攻撃へ移行
	}

	// **LShiftを離したら通常行動に戻る**
	if (!input_->PushKey(DIK_LSHIFT))
	{
		behaviorRequest_ = Behavior::kRoot;
		return;
	}

	if (input_->TriggerKey(DIK_SPACE))
	{
		behaviorRequest_ = Behavior::kJump; // ジャンプ開始
		return;
	}

	// **移動量を初期化**
	Vector3 movement = { 0.0f, 0.0f, 0.0f };

	// **入力取得 (移動)**
	if (input_->PushKey(DIK_W)) { movement.z += 2.0f; }
	if (input_->PushKey(DIK_S)) { movement.z -= 2.0f; }
	if (input_->PushKey(DIK_A)) { movement.x -= 2.0f; }
	if (input_->PushKey(DIK_D)) { movement.x += 2.0f; }

	float targetYaw = parts_[0].worldTransform.rotate.y; // 現在の角度を基準に

	// **カメラの向きを考慮した移動方向を計算**
	if (camera_)
	{
		Vector3 forward = camera_->GetForwardDirection();
		forward.y = 0.0f; // 水平移動のためY成分をゼロにする

		if (Length(forward) > 0.001f)
		{
			forward = Normalize(forward);
		}
		else
		{
			forward = Vector3(0.0f, 0.0f, 1.0f); // デフォルト方向を設定
		}

		Vector3 right = Normalize(Cross(Vector3(0.0f, 1.0f, 0.0f), forward)); // 水平方向の右ベクトル

		Vector3 adjustedMovement = -forward * movement.z + -right * movement.x;
		adjustedMovement.y = 0.0f;

		// **滑らかに加速＆減速**
		velocity = Lerp(velocity, adjustedMovement * dashSpeed, 1.0f);

		// **胴体（親）の移動を適用**
		parts_[0].worldTransform.translate += velocity;

		// **カメラのターゲット位置も遅れて移動**
		camera_->SetTargetPosition(Lerp(camera_->GetTargetPosition(), parts_[0].worldTransform.translate, 0.1f));


		// **移動している場合のみ目標角度を計算**
		if (Length(adjustedMovement) > 0.001f)
		{
			Vector3 forwardDirection = Normalize(adjustedMovement);
			targetYaw = atan2f(-forwardDirection.x, forwardDirection.z); // 移動方向を向く
		}
	}

	// **現在の角度と目標角度を補間**
	float currentYaw = parts_[0].worldTransform.rotate.y;
	parts_[0].worldTransform.rotate.y = LerpShortAngle(currentYaw, targetYaw, 0.1f); // t=0.1で補間

	// **腕と武器の位置を胴体に同期**
	for (size_t i = 1; i < parts_.size(); ++i)
	{
		int parentIndex = parts_[i].parentIndex;
		if (parentIndex != -1)
		{
			const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
			Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
			Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

			parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
			parts_[i].worldTransform.rotate.y = parentRotation.y; // Y回転のみ親と同じにする
		}
	}

	// **各部位のオブジェクトの位置と回転を更新**
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// **プレイヤーの位置をカメラに反映**
	if (camera_)
	{
		camera_->SetTargetPosition(parts_[0].worldTransform.translate);
		camera_->Update();
	}
}


/// -------------------------------------------------------------
///					 攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorJumpInitialize()
{
	// **ジャンプ初速を設定**
	const float kJumpFirstSpeed = 1.0f; // 少し低めに調整
	velocity.y = kJumpFirstSpeed;

	// **ジャンプ開始**
	behavior_ = Behavior::kJump;
}


/// -------------------------------------------------------------
///					 攻撃行動の更新処理
/// -------------------------------------------------------------
void Player::BehaviorJumpUpdate()
{
	const float kGravityAcceleration = 0.03f; // 重力加速度を調整（滑らかな落下のために少し増加）
	const float kHorizontalSpeed = 0.3f;      // 水平方向の移動速度を抑える
	const float kMaxFallSpeed = -2.5f;        // 最大落下速度（制限）
	const float groundHeight = 0.0f;          // 地面の高さ

	// **重力の影響をY方向速度に加える**
	velocity.y -= kGravityAcceleration;

	// **降下速度の制限を適用**
	if (velocity.y < kMaxFallSpeed)
	{
		velocity.y = kMaxFallSpeed;
	}

	// **移動量を初期化**
	Vector3 movement = { 0.0f, 0.0f, 0.0f };

	// **入力取得（移動）**
	if (input_->PushKey(DIK_W)) { movement.z += 1.0f; }
	if (input_->PushKey(DIK_S)) { movement.z -= 1.0f; }
	if (input_->PushKey(DIK_A)) { movement.x -= 1.0f; }
	if (input_->PushKey(DIK_D)) { movement.x += 1.0f; }

	// **カメラの向きを考慮して移動方向を計算**
	float targetYaw = parts_[0].worldTransform.rotate.y; // 現在の角度を基準に

	// カメラの向きに基づいて移動方向を計算
	if (camera_)
	{
		Vector3 forward = camera_->GetForwardDirection();
		forward.y = 0.0f; // 水平移動のためY成分をゼロにする

		if (Length(forward) > 0.001f) {
			forward = Normalize(forward);
		}
		else {
			forward = Vector3(0.0f, 0.0f, 1.0f); // デフォルト方向を設定
		}

		Vector3 right = Normalize(Cross(Vector3(0.0f, 1.0f, 0.0f), forward)); // 水平方向の右ベクトル

		Vector3 adjustedMovement = -forward * movement.z + -right * movement.x;
		adjustedMovement.y = 0.0f;

		// 胴体（親）の移動を適用
		parts_[0].worldTransform.translate += adjustedMovement * 0.3f; // 移動速度を調整

		// 移動している場合のみ目標角度を計算
		if (Length(adjustedMovement) > 0.001f)
		{
			Vector3 forwardDirection = Normalize(adjustedMovement);
			targetYaw = atan2f(-forwardDirection.x, forwardDirection.z); // 移動方向を向く
		}
	}

	// 現在の角度と目標角度を補間
	float currentYaw = parts_[0].worldTransform.rotate.y;
	parts_[0].worldTransform.rotate.y = LerpShortAngle(currentYaw, targetYaw, 0.1f); // t=0.1で補間

	// 腕の揺れのパラメータを常に更新
	armSwingParameter_ += step * armSwingSpeed_;

	// 揺れの大きさを移動量に応じて変化させる（移動時は大きく、静止時は小さめ）
	float swingAmplitude = (Length(movement) > 0.001f) ? armSwingAmplitude_ : armSwingAmplitude_ * 0.5f;

	// 移動しているかどうかで揺れ方を変える
	float leftArmSwing, rightArmSwing;
	if (Length(movement) > 0.001f) {
		// 移動時：交互に揺れる
		leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
		rightArmSwing = std::sin(armSwingParameter_ + PI) * swingAmplitude;
	}
	else {
		// 静止時：同じ方向に揺れる
		leftArmSwing = std::sin(armSwingParameter_) * swingAmplitude;
		rightArmSwing = leftArmSwing; // 左右同じ揺れ
	}

	// 両腕の回転に適用
	parts_[1].worldTransform.rotate.x = leftArmSwing;  // 左腕
	parts_[2].worldTransform.rotate.x = rightArmSwing; // 右腕

	// **垂直方向の移動を適用**
	parts_[0].worldTransform.translate.y += velocity.y;

	// **着地判定**
	if (parts_[0].worldTransform.translate.y <= groundHeight)
	{
		parts_[0].worldTransform.translate.y = groundHeight; // 地面に固定
		velocity.y = 0.0f;                                  // 垂直速度をリセット
		behaviorRequest_ = Behavior::kRoot;                 // 通常行動に戻る
	}

	// **各部位の最終的な位置と回転を計算**
	for (size_t i = 1; i < parts_.size(); ++i)
	{
		int parentIndex = parts_[i].parentIndex;
		if (parentIndex != -1)
		{
			const Vector3& parentRotation = parts_[parentIndex].worldTransform.rotate;
			Matrix4x4 rotationMatrix = MakeRotateYMatrix(parentRotation.y);
			Vector3 rotatedOffset = Transform(parts_[i].localOffset, rotationMatrix);

			parts_[i].worldTransform.translate = parts_[parentIndex].worldTransform.translate + rotatedOffset;
			parts_[i].worldTransform.rotate.y = parentRotation.y; // Y回転のみ親と同じにする
		}
	}

	// **各部位のオブジェクトの位置と回転を更新**
	for (auto& part : parts_)
	{
		part.object3D->SetRotate(part.worldTransform.rotate);
		part.object3D->SetTranslate(part.worldTransform.translate);
		part.object3D->Update();
	}

	// **カメラ位置をプレイヤーに追従**
	if (camera_)
	{
		camera_->SetTargetPosition(parts_[0].worldTransform.translate);
		camera_->Update();
	}
}
