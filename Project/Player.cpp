#include "Player.h"
#include "Object3DCommon.h"
#include <Input.h>
#include <Floor.h>
#include <ObstacleManager.h>
#include <SceneManager.h>

/// -------------------------------------------------------------
///                     初期化処理
/// -------------------------------------------------------------
void Player::Initialize(Object3DCommon* object3DCommon)
{
	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Initialize(object3DCommon, "plane.obj");
	transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f, 0.0f,0.0f} };
	playerObject_->SetTranslate(transform_.translate);
}

/// -------------------------------------------------------------
///                     更新処理
/// -------------------------------------------------------------
void Player::Update(Input* input, Floor* floor, const ObstacleManager* obstacleManager)
{
	Handle(input, floor);

	// 実行中の衝突判定
	if (CheckCollisionWithObstacles(obstacleManager))
	{
		OutputDebugStringA("Collision detected!\n");  // 衝突時の処理

		// シーン遷移をリクエスト
		SceneManager::GetInstance()->ChangeScene("GameResultScene");
	}

	// カメラのターゲットをプレイヤー位置に設定
	if (camera_)
	{
		camera_->SetTargetPosition(transform_.translate);
	}

	// 位置を更新
	playerObject_->SetTranslate(transform_.translate);
	playerObject_->SetRotate(transform_.rotate);

	// プレイヤーオブジェクトの更新
	playerObject_->Update();
}

/// -------------------------------------------------------------
///                     描画処理
/// -------------------------------------------------------------
void Player::Draw()
{
	playerObject_->Draw();
}

/// -------------------------------------------------------------
///                     衝突判定
/// -------------------------------------------------------------
bool Player::CheckCollisionWithObstacles(const ObstacleManager* obstacleManager)
{
	if (!obstacleManager) return false;

	// プレイヤーのAABBを計算
	const float playerMinX = transform_.translate.x - 0.5f;  // X最小
	const float playerMaxX = transform_.translate.x + 0.5f;  // X最大
	const float playerMinY = transform_.translate.y - 0.5f;  // Y最小
	const float playerMaxY = transform_.translate.y + 0.5f;  // Y最大
	const float playerMinZ = transform_.translate.z - 0.5f;  // Z最小
	const float playerMaxZ = transform_.translate.z + 0.5f;  // Z最大

	for (const auto& obstacle : obstacleManager->GetObstacles())
	{
		// 障害物のAABBを計算
		const Transform& obstacleTransform = obstacle.GetTransform();
		const float obstacleMinX = obstacleTransform.translate.x - 0.5f;
		const float obstacleMaxX = obstacleTransform.translate.x + 0.5f;
		const float obstacleMinY = obstacleTransform.translate.y - 0.5f;
		const float obstacleMaxY = obstacleTransform.translate.y + 0.5f;
		const float obstacleMinZ = obstacleTransform.translate.z - 0.5f;
		const float obstacleMaxZ = obstacleTransform.translate.z + 0.5f;

		// AABB判定
		if (playerMaxX > obstacleMinX && playerMinX < obstacleMaxX &&  // X軸の重なり
			playerMaxY > obstacleMinY && playerMinY < obstacleMaxY &&  // Y軸の重なり
			playerMaxZ > obstacleMinZ && playerMinZ < obstacleMaxZ)    // Z軸の重なり
		{
			return true;  // 衝突を検出
		}
	}

	return false;  // 衝突なし
}

void Player::Handle(Input* input, Floor* floor)
{
	// レーン変更
	if (input->TriggerKey(DIK_RIGHT) && laneInfo_.currentIndex < 2)
	{
		laneInfo_.currentIndex++;
		rotationInfo_ = { false, 0.0f, 10.0f, 0 }; // 回転状態のリセット
	}

	if (input->TriggerKey(DIK_LEFT) && laneInfo_.currentIndex > 0)
	{
		laneInfo_.currentIndex--;
		rotationInfo_ = { false, 0.0f, 10.0f, 0 }; // 回転状態のリセット
	}

	// 目標位置を取得
	float targetX = laneInfo_.positions[laneInfo_.currentIndex];

	// 線形補間で現在位置を更新
	transform_.translate.x += (targetX - transform_.translate.x) * laneInfo_.moveSpeed;

	// 底の高さを取得し、プレイヤーのY座標を固定
	if (floor)
	{
		float floorHeight = floor->GetFloorHeightAt(transform_.translate.x, transform_.translate.z);
		transform_.translate.y = floorHeight; // 底の高さにY座標を合わせる
	}

	// ジャンプ処理
	if (input->PushKey(DIK_UP) && !jumpInfo_.isJumping)
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

	transform_.translate.y = jumpInfo_.height; // Y座標を更新

	// 回転処理
	if (input->TriggerKey(DIK_DOWN) && !rotationInfo_.isRotating)
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

	transform_.rotate.x = rotationInfo_.angle; // X軸回転角を更新
}
