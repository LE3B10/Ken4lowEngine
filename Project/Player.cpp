#include "Player.h"
#include "Object3DCommon.h"
#include <Input.h>

void Player::Initialize(Object3DCommon* object3DCommon)
{
	playerObject_ = std::make_unique<Object3D>();
	playerObject_->Initialize(object3DCommon, "plane.obj");
	transform_ = { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,-1.0f,0.0f} };
	playerObject_->SetTranslate(transform_.translate);
}

void Player::Update(Input* input)
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

    // ジャンプ処理
    if (input->PushKey(DIK_UP) && !jumpInfo_.isJumping)
    {
        // ジャンプ開始時に回転状態をリセット
        jumpInfo_.isJumping = true;
        jumpInfo_.height = -1.0f;

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
        if (jumpInfo_.height <= -1.0f)
        {
            jumpInfo_.height = -1.0f;
            jumpInfo_.velocity = 0.2f;  // 初速度をリセット
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
