#include "RunningBehavior.h"
#include "Player.h"
#include "Input.h"
#include "AnimationModel.h"

void RunningBehavior::Initialize(Player* player)
{
	// 初期化処理は特に必要ないが、将来の拡張のために残しておく
	AnimationModel* model = player->GetAnimationModel();
	model->Initialize("PlayerStateModel/PlayerRunState.gltf"); // モデルの初期化
	model->Update(); // アニメーションモデルの更新
	player->SetCurrentState(ModelState::Running); // 状態をWalkingに設定
}

void RunningBehavior::Update(Player* player)
{
	Input* input = player->GetInput();
	AnimationModel* model = player->GetAnimationModel();

	Vector3 move{};

	if (input->PushKey(DIK_W)) { move.z += 1.0f; }
	if (input->PushKey(DIK_S)) { move.z -= 1.0f; }
	if (input->PushKey(DIK_A)) { move.x -= 1.0f; }
	if (input->PushKey(DIK_D)) { move.x += 1.0f; }

	// ジャンプキーの入力をチェック
	bool isJumping = input->PushKey(DIK_SPACE);

	if (Vector3::Length(move) > 0.0f)
	{
		// Space が押されていればジャンプに遷移
		if (isJumping)
		{
			player->SetCurrentState(ModelState::Jumping);
			return;
		}

		move = Vector3::Normalize(move);
		const float speed = 0.1f; // 走行速度を上げる
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

void RunningBehavior::Draw(Player* player)
{
	// 描画処理はアニメーションモデルに任せる
	player->GetAnimationModel()->Draw();
}
