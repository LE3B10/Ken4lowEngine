#include "Player.h"
#include "Object3DCommon.h"
#include "CollisionTypeIdDef.h"

#include <SceneManager.h>


Player::Player()
{
	// シリアルナンバーを振る
	serialNumber_ = nextSerialNumber_;
	// 次のシリアルナンバーに１を足す
	++nextSerialNumber_;
}


/// -------------------------------------------------------------
///                     初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon)
{
	// 衝突判定の初期化
	Collider::Initialize(object3DCommon);
	Collider::SetTranslate(worldTransform_.translate);

	input = Input::GetInstance();

	// オブジェクトの生成と初期化
	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Initialize(object3DCommon, "player.gltf");
	worldTransform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f, 0.0f,0.0f} };
	playerObject_->SetTranslate(worldTransform_.translate);

	// 識別番号（ID）の設定
	Collider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
}


/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void Player::Update()
{
	Collider::SetTranslate(worldTransform_.translate);
	// 衝突判定の更新処理
	Collider::Update();

	// レーン変更
	if ((input->TriggerKey(DIK_RIGHT) || input->TriggerKey(DIK_D)) && laneInfo_.currentIndex < 2)
	{
		laneInfo_.currentIndex++;
		rotationInfo_ = { false, 0.0f, 10.0f, 0 }; // 回転状態のリセット
	}

	if ((input->TriggerKey(DIK_LEFT) || input->TriggerKey(DIK_A)) && laneInfo_.currentIndex > 0)
	{
		laneInfo_.currentIndex--;
		rotationInfo_ = { false, 0.0f, 10.0f, 0 }; // 回転状態のリセット
	}

	// 目標位置を取得
	float targetX = laneInfo_.positions[laneInfo_.currentIndex];

	// 線形補間で現在位置を更新
	worldTransform_.translate.x += (targetX - worldTransform_.translate.x) * laneInfo_.moveSpeed;

	// ジャンプ処理
	if ((input->PushKey(DIK_UP) || input->TriggerKey(DIK_W) || input->TriggerKey(DIK_SPACE)) && !jumpInfo_.isJumping)
	{
		jumpInfo_ = { true, 0.25f, -0.01f, 0.0f }; // ジャンプ開始
	}

	if (jumpInfo_.isJumping)
	{
		// 落下速度増加処理
		jumpInfo_.gravity = rotationInfo_.isRotating ? -0.03f : -0.01f;
		jumpInfo_.height += jumpInfo_.velocity;  // ジャンプの上昇
		jumpInfo_.velocity += jumpInfo_.gravity; // 重力を加算

		// 地面に到達したら停止
		if (jumpInfo_.height <= 0.0f)
		{
			jumpInfo_ = { false, 0.25f, -0.01f, 0.0f }; // ジャンプ閉上
		}
	}

	worldTransform_.translate.y = jumpInfo_.height; // Y座標を更新

	// 回転処理
	if ((input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) && !rotationInfo_.isRotating)
	{
		rotationInfo_ = { true, 0.0f, 10.0f, 0 }; // 回転開始
	}

	if (rotationInfo_.isRotating)
	{
		rotationInfo_.angle += rotationInfo_.speed; // 回転速度分回転
		if (rotationInfo_.angle >= 180.0f)
		{
			rotationInfo_.angle -= 180.0f;
			rotationInfo_.count++;
		}

		if (rotationInfo_.count >= 2) // 2回転で終了
		{
			rotationInfo_ = { false, 0.0f, 10.0f, 0 };
		}
	}

	worldTransform_.rotate.x = rotationInfo_.angle; // X軸回転角を更新

	// 位置を更新
	playerObject_->SetTranslate(worldTransform_.translate);
	playerObject_->SetRotate(worldTransform_.rotate);

	// プレイヤーオブジェクトの更新
	playerObject_->Update();
}


/// -------------------------------------------------------------
///							描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	// 描画処理
	playerObject_->Draw();
}


/// -------------------------------------------------------------
///                     接触記録を削除
/// -------------------------------------------------------------
void Player::ClearContactRecord()
{
	// 接触履歴を削除
	contactRecord_.Clear();
}


/// -------------------------------------------------------------
///                     衝突判定が呼ばれる処理
/// -------------------------------------------------------------
void Player::OnCollision(Collider* other)
{
	// 衝突相手の識別番号（ID）を取得
	uint32_t typeID = other->GetTypeID();

	// 衝突相手が床なら
	if (typeID == static_cast<uint32_t>(CollisionTypeIdDef::kFloorBlock))
	{
		// 床の変数

		// シリアルナンバー
		uint32_t serialNumber = 0; // 仮

		// 接触記録があれば何もせずに抜ける
		if (contactRecord_.Check(serialNumber))
		{
			return;
		}

		// 接触記録に登録
		contactRecord_.Add(serialNumber);

		// 必要であればエフェクトを追加

	}
}


/// -------------------------------------------------------------
///                     衝突判定が呼ばれる処理
/// -------------------------------------------------------------
Vector3 Player::GetOrientation(int index) const
{
	return Vector3();
}
