#include "WalkingBehavior.h"
#include "Player.h"
#include "Input.h"
#include "Vector3.h"

void WalkingBehavior::Initialize(Player* player)
{
	// 初期化処理は特に必要ないが、将来の拡張のために残しておく
	AnimationModel* model = player->GetAnimationModel();
	model->Initialize("PlayerStateModel/humanWalking.gltf"); // モデルの初期化
	model->Update(); // アニメーションモデルの更新
	player->SetCurrentState(ModelState::Walking); // 状態をWalkingに設定
}

void WalkingBehavior::Update(Player* player)
{
	Input* input = player->GetInput();
	AnimationModel* model = player->GetAnimationModel();

	Vector3 move{};

	if (input->PushKey(DIK_W)) { move.z += 1.0f; }
	if (input->PushKey(DIK_S)) { move.z -= 1.0f; }
	if (input->PushKey(DIK_A)) { move.x -= 1.0f; }
	if (input->PushKey(DIK_D)) { move.x += 1.0f; }

	// --- ゲームパッド入力（左スティック）---
	if (!input->LStickInDeadZone())
	{
		Vector2 stick = input->GetLeftStick(); // x=左右, y=上下
		move.x += stick.x;
		move.z += stick.y;
	}

	// 走行キーの入力をチェック
	bool isRunning = input->TriggerKey(DIK_LSHIFT);

	// --- ゲームパッド入力（走行ボタン）---
	if (input->TriggerButton(XButtons.B)) // Bボタンで走行
	{
		isRunning = true;
	}

	// ジャンプキーの入力をチェック
	bool isJumping = input->PushKey(DIK_SPACE);

	if (Vector3::Length(move) > 0.0f)
	{
		// LShift が押されていれば走行に遷移
		if (isRunning)
		{
			player->SetCurrentState(ModelState::Running);
			return;
		}

		// Space が押されていればジャンプに遷移
		if (isJumping)
		{
			player->SetCurrentState(ModelState::Jumping);
			return;
		}

		move = Vector3::Normalize(move);
		const float speed = 0.03f;

		// 現在座標に移動を加算
		Vector3 currentPos = model->GetTranslate();
		model->SetTranslate(currentPos + move * speed);

		// 向きも移動方向に応じて回転（Y軸角度に変換）
		float angle = std::atan2(-move.x, move.z); // Y軸回転
		model->SetRotate(Vector3(0, angle, 0));
	}
	else
	{
		// 移動が無ければIdleに戻す
		player->SetCurrentState(ModelState::Idle);
	}

	model->Update(); // アニメーションモデルの更新
}

void WalkingBehavior::Draw(Player* player)
{
	// 描画処理はアニメーションモデルに任せる
	player->GetAnimationModel()->Draw();
}
