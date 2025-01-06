#include "Player.h"
#include "Object3DCommon.h"
#include <Input.h>
#include <Floor.h>
#include <ObstacleManager.h>

void Player::Initialize(Object3DCommon* object3DCommon)
{
	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Initialize(object3DCommon, "plane.obj");
	transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f, 0.0f,0.0f} };
	playerObject_->SetTranslate(transform_.translate);
}

void Player::Update(Input* input, Floor* floor, const ObstacleManager* obstacleManager)
{
	// レーン変更
	if (input->TriggerKey(DIK_RIGHT) && laneInfo_.currentIndex < 2)
	{
		laneInfo_.currentIndex++;

		// 回転状態をリセット
		rotationInfo_.isRotating = false;
		rotationInfo_.angle = 0.0f;
		rotationInfo_.count = 0;
	}

	if (input->TriggerKey(DIK_LEFT) && laneInfo_.currentIndex > 0)
	{
		laneInfo_.currentIndex--;

		// 回転状態をリセット
		rotationInfo_.isRotating = false;
		rotationInfo_.angle = 0.0f;
		rotationInfo_.count = 0;
	}

	// 目標位置を取得
	float targetX = laneInfo_.positions[laneInfo_.currentIndex];

	// 線形補間で現在位置を更新
	transform_.translate.x += (targetX - transform_.translate.x) * laneInfo_.moveSpeed;

	// 床の高さを取得し、プレイヤーのY座標を固定
	if (floor)
	{
		float floorHeight = floor->GetFloorHeightAt(transform_.translate.x, transform_.translate.z);
		transform_.translate.y = floorHeight; // 床の高さにY座標を合わせる
	}

	// ジャンプ処理
	if (input->PushKey(DIK_UP) && !jumpInfo_.isJumping)
	{
		// ジャンプ開始時に回転状態をリセット
		jumpInfo_.isJumping = true;
		jumpInfo_.height = 0.0f;

		// 回転状態をリセット
		rotationInfo_.isRotating = false;
		rotationInfo_.angle = 0.0f;
		rotationInfo_.count = 0;
	}

	if (jumpInfo_.isJumping)
	{
		// 落下速度増加処理（空中で回転中の場合）
		if (rotationInfo_.isRotating)
		{
			jumpInfo_.gravity = -0.03f; // 通常の倍の重力加速度
		}
		else
		{
			jumpInfo_.gravity = -0.01f; // 通常の重力加速度
		}

		jumpInfo_.height += jumpInfo_.velocity;  // ジャンプの上昇
		jumpInfo_.velocity += jumpInfo_.gravity; // 重力を加算

		// 地面に到達したら停止
		if (jumpInfo_.height <= 0.0f)
		{
			jumpInfo_.height = 0.0f;
			jumpInfo_.velocity = 0.25f;  // 初速度をリセット
			jumpInfo_.isJumping = false;
		}
	}

	transform_.translate.y = jumpInfo_.height; // Y座標を更新

	// 回転処理
	if (input->TriggerKey(DIK_DOWN) && !rotationInfo_.isRotating)
	{
		rotationInfo_.isRotating = true;
		rotationInfo_.angle = 0.0f;
		rotationInfo_.count = 0;
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
			rotationInfo_.isRotating = false;
			rotationInfo_.angle = 0.0f;
		}
	}

	transform_.rotate.x = rotationInfo_.angle; // X軸回転角を更新

	if (CheckCollisionWithObstacles(obstacleManager))
	{
		OutputDebugStringA("Collision detected!\n");  // 衝突時の処理
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

void Player::Draw()
{
	playerObject_->Draw();
}

bool Player::CheckCollisionWithObstacles(const ObstacleManager* obstacleManager)
{
	if (!obstacleManager) return false;

	const float collisionThresholdX = 1.0f;  // X方向の衝突閾値
	const float collisionThresholdZ = 1.0f;  // Z方向の衝突閾値

	for (const auto& obstacle : obstacleManager->GetObstacles())
	{
		const Transform& obstacleTransform = obstacle.GetTransform();

		// XとZの差が閾値以内であれば衝突と判定
		if (std::abs(obstacleTransform.translate.x - transform_.translate.x) < collisionThresholdX &&
			std::abs(obstacleTransform.translate.z - transform_.translate.z) < collisionThresholdZ)
		{
			return true;  // 衝突を検出
		}
	}
	return false;  // 衝突なし
}