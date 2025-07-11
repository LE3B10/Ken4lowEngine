#include "JumpingBehavior.h"
#include "Player.h"
#include "Input.h"
#include "AnimationModel.h"


void JumpingBehavior::Initialize(Player* player)
{
	// 初期化処理は特に必要ないが、将来の拡張のために残しておく
	AnimationModel* model = player->GetAnimationModel();
	model->Initialize("PlayerStateModel/PlayerJumpingState.gltf"); // モデルの初期化
	model->Update(); // アニメーションモデルの更新
	player->SetCurrentState(ModelState::Jumping); // 状態をWalkingに設定

	// ←追加ここから
	jumpVelocity_ = 0.2f;    // ジャンプ初速度
	isFalling_ = false;
	gravity_ = 0.008f;        // 重力加速度
	groundY_ = model->GetTranslate().y; // 現在の地面高さ
}

void JumpingBehavior::Update(Player* player)
{
	Input* input = player->GetInput();
	AnimationModel* model = player->GetAnimationModel();
	Vector3 position = model->GetTranslate();

	// --- 横移動処理（空中でも移動可能にする） ---
	Vector3 move{};
	if (input->PushKey(DIK_W)) { move.z += 1.0f; }
	if (input->PushKey(DIK_S)) { move.z -= 1.0f; }
	if (input->PushKey(DIK_A)) { move.x -= 1.0f; }
	if (input->PushKey(DIK_D)) { move.x += 1.0f; }

	if (Vector3::Length(move) > 0.0f)
	{
		move = Vector3::Normalize(move);
		const float airSpeed = 0.05f; // 空中での移動速度（走行より遅めでもOK）
		position += move * airSpeed;

		// 回転の適用（方向に向かせる）
		float angle = std::atan2(-move.x, move.z);
		model->SetRotate(Vector3(0, angle, 0));
	}

	// --- 上下ジャンプ処理 ---
	if (!isFalling_)
	{
		position.y += jumpVelocity_;
		jumpVelocity_ -= gravity_; // 上昇の減速

		if (jumpVelocity_ <= 0.0f)
		{
			isFalling_ = true;
		}
	}
	else
	{
		position.y -= jumpVelocity_;
		jumpVelocity_ += gravity_; // 落下の加速

		if (position.y <= groundY_)
		{
			position.y = groundY_;

			// ←地上着地後、移動入力の有無で状態を分岐して復帰させる
			Vector3 groundMove{};
			if (input->PushKey(DIK_W) || input->PushKey(DIK_S) || input->PushKey(DIK_A) || input->PushKey(DIK_D))
			{
				if (input->PushKey(DIK_LSHIFT)) {
					player->SetCurrentState(ModelState::Running);
				}
				else {
					player->SetCurrentState(ModelState::Walking);
				}
			}
			else {
				player->SetCurrentState(ModelState::Idle);
			}
			return;
		}
	}

	model->SetTranslate(position);
	model->Update();
}

void JumpingBehavior::Draw(Player* player)
{
	// 描画処理はアニメーションモデルに任せる
	player->GetAnimationModel()->Draw();
}
